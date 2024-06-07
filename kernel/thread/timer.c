
#include <timer.h>
#include <spinlock.h>
#include <assert.h>
#include <cpu.h>
#include <panic.h>
#include <irql.h>
#include <errno.h>
#include <log.h>
#include <arch.h>
#include <thread.h>
#include <ksignal.h>
#include <priorityqueue.h>
#include <threadlist.h>

#define EXPIRED_ALARM ((uint64_t) -1)
#define MAX_ALARMS 256

struct alarm {
    uint64_t wakeup_time;
    void (*callback)(void*);
    void* arg;
};

static struct alarm alarms[MAX_ALARMS];
static int num_alarms_installed = 0;

static struct spinlock timer_lock;
static struct thread_list sleep_list;

static uint64_t system_time = 0;
static int sleep_wakeups_posted = 0;

void ReceivedTimer(uint64_t nanos) {
    EXACT_IRQL(IRQL_TIMER);

    if (ArchGetCurrentCpuIndex() == 0) {
        /*
         * Although we are in IRQL_TIMER, we must still lock to prevent other 
         * CPUs from reading at a bad time. 
         */
        AcquireSpinlock(&timer_lock);
        system_time += nanos;
        ReleaseSpinlock(&timer_lock);
    }

    /*
     * Preempt the current thread if it has used up its timeslice. 
     */
    struct thread* thr = GetThread();
    if (thr != NULL && thr->timeslice_expiry != 0 && thr->timeslice_expiry <= system_time) {
        PostponeScheduleUntilStandardIrql();
    }

    if (GetNumberInDeferQueue() < 8) {
        DeferUntilIrql(IRQL_STANDARD, HandleSleepWakeups, (void*) &system_time);
    }
}

uint64_t GetSystemTimer(void) {    
    AcquireSpinlock(&timer_lock);
    uint64_t value = system_time;
    ReleaseSpinlock(&timer_lock);
    return value;
}

void InitTimer(void) {
    InitSpinlock(&timer_lock, "timer", IRQL_TIMER);
    ThreadListInit(&sleep_list, NEXT_INDEX_SLEEP);
}

void QueueForSleep(struct thread* thr) {
    AssertSchedulerLockHeld();
    thr->timed_out = false;
    ThreadListInsert(&sleep_list, thr);
}

bool TryDequeueForSleep(struct thread* thr) {
    AssertSchedulerLockHeld();
    
    struct thread* iter = sleep_list.head;
    while (iter) {
        if (iter == thr) {
            ThreadListDelete(&sleep_list, iter);
            return true;

        } else {
            iter = iter->next[NEXT_INDEX_SLEEP];
        }
    }

    return false;
}

static void HandleAlarms(uint64_t time) {
    AssertSchedulerLockHeld();
    for (int i = 0; i < MAX_ALARMS && num_alarms_installed > 0; ++i) {
        if (alarms[i].wakeup_time <= time) {
            alarms[i].wakeup_time = EXPIRED_ALARM;
            num_alarms_installed--;
            DeferUntilIrql(IRQL_STANDARD, alarms[i].callback, alarms[i].arg);
        }
    }
}

void HandleSleepWakeups(void* sys_time_ptr) {
    EXACT_IRQL(IRQL_STANDARD);

    if (GetThread() == NULL) {
        return;
    }

    LockScheduler();
    if (sleep_wakeups_posted > 0) {
        --sleep_wakeups_posted;
    }

    uint64_t system_time = *((uint64_t*) sys_time_ptr);

    HandleAlarms(system_time);

    struct thread* iter = sleep_list.head;
    while (iter) {
        if (iter->sleep_expiry <= system_time || iter->signal_intr) {
            ThreadListDelete(&sleep_list, iter);
            iter->timed_out = true;
            iter->timed_out_due_to_signal = iter->signal_intr;
            UnblockThread(iter);
            iter = sleep_list.head;     // restart, as list changed

        } else {
            iter = iter->next[NEXT_INDEX_SLEEP];
        }
    }

    UnlockScheduler();
}

int SleepUntil(uint64_t system_time_ns) {
    EXACT_IRQL(IRQL_STANDARD);

    if (system_time_ns < GetSystemTimer()) {
        return 0;
    }

    LockScheduler();
    GetThread()->sleep_expiry = system_time_ns;
    QueueForSleep(GetThread());
    BlockThread(THREAD_STATE_SLEEPING);
    UnlockScheduler();

    return GetThread()->timed_out_due_to_signal ? EINTR : 0;
}

int SleepNano(uint64_t delta_ns) {
    return SleepUntil(GetSystemTimer() + delta_ns);
}

int SleepMilli(uint32_t delta_ms) {
    return SleepNano(((uint64_t) delta_ms) * 1000000ULL);
}

int CreateAlarmAbsolute(uint64_t system_time_ns, void (*callback)(void*), void* arg, int* id_out) {
    if (system_time_ns < GetSystemTimer()) {
        *id_out = -1;
        return EALREADY;
    }

    LockScheduler();
    if (num_alarms_installed >= MAX_ALARMS) {
        UnlockScheduler();
        *id_out = -1;
        return EAGAIN;
    }

    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (alarms[i].wakeup_time == 0) {
            alarms[i].wakeup_time = system_time_ns;
            alarms[i].arg = arg;
            alarms[i].callback = callback;
            *id_out = i;
            break;
        }
    }

    ++num_alarms_installed;

    UnlockScheduler();
    return 0;
}

int CreateAlarmMicro(uint64_t delta_us, void (*callback)(void*), void* arg, int* id_out) {
    return CreateAlarmAbsolute(GetSystemTimer() + delta_us * 1000ULL, callback, arg, id_out);
}

static int UpdateAlarm(int id, uint64_t* time_left_out, bool destroy) {
    if (id >= MAX_ALARMS) {
        return EINVAL;
    }
    LockScheduler();
    uint64_t sys_time = GetSystemTimer();
    int retv = 0;

    if (alarms[id].wakeup_time == 0) {
        retv = EINVAL;
    } else {
        if (alarms[id].wakeup_time <= sys_time) {
            /*
             * In the time that they called `DestoryAlarm`, it went off. That's
             * an odd situation, so just pretend it's still got a nanosecond to
             * go, and hopefully the caller will retry.
             */
            *time_left_out = 1;

        } else if (alarms[id].wakeup_time == EXPIRED_ALARM) {
            *time_left_out = 0;
        } else {
            *time_left_out = alarms[id].wakeup_time - sys_time;
        }

        if (destroy) {
            alarms[id].wakeup_time = 0;
        }
    }

    UnlockScheduler();
    return retv;
}

int GetAlarmTimeRemaining(int id, uint64_t* time_left_out) {
    return UpdateAlarm(id, time_left_out, false);
}

int DestroyAlarm(int id, uint64_t* time_left_out) {
    return UpdateAlarm(id, time_left_out, true);
}

static void UnixAlarmHandler(void* arg) {
    uint64_t rem;
    struct thread* thr = (struct thread*) arg;
    RaiseSignal(thr, SIGALRM, false);
    DestroyAlarm(thr->alarm_id, &rem);
}

int InstallUnixAlarm(uint64_t microseconds, uint64_t* remaining_microsecs) {
    LockScheduler();
    uint64_t time_left;
    int res = DestroyAlarm(GetThread()->alarm_id, &time_left);
    GetThread()->alarm_id = -1;
    if (res == 0) {
        *remaining_microsecs = (time_left + 999ULL) / 1000ULL;
    }
    UnlockScheduler();
    if (res != 0) {
        return EINVAL;
    }
    if (microseconds != 0) {
        res = CreateAlarmMicro(microseconds, UnixAlarmHandler, GetThread(), &(GetThread()->alarm_id));
        if (res != 0) {
            return res;
        }
    }
    return 0;
}