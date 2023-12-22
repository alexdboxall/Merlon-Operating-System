
#include <arch.h>
#include <irql.h>
#include <thread.h>
#include <assert.h>
#include <virtual.h>
#include <sys/types.h>
#include <avl.h>
#include <panic.h>
#include <semaphore.h>
#include <spinlock.h>
#include <heap.h>
#include <process.h>
#include <log.h>
#include <linkedlist.h>

struct process {
    pid_t pid;
    struct vas* vas;
    pid_t parent;
    struct avl_tree* children;
    struct avl_tree* threads;
    struct semaphore* lock;
    struct semaphore* killed_children_semaphore;
    int retv;
    bool terminated;
};

struct process_table_node {
    pid_t pid;
    struct process* process;
};

static struct spinlock pid_lock;
static pid_t next_pid = 1;
static struct avl_tree* process_table;
static struct semaphore* process_table_mutex;

static int ProcessTableComparator(void* a_, void* b_) {
    struct process_table_node* a = a_;
    struct process_table_node* b = b_;

    if (a->pid == b->pid) {
        return 0;
    }
    
    return a->pid < b->pid ? -1 : 1;
}

static int PAGEABLE_CODE_SECTION InsertIntoProcessTable(struct process* prcss) {
    MAX_IRQL(IRQL_STANDARD);

    AcquireSpinlockIrql(&pid_lock);
    pid_t pid = next_pid++;
    ReleaseSpinlockIrql(&pid_lock);

    AcquireMutex(process_table_mutex, -1);

    struct process_table_node* node = AllocHeap(sizeof(struct process_table_node));
    node->pid = pid;
    node->process = prcss;
    AvlTreeInsert(process_table, (void*) node);

    ReleaseMutex(process_table_mutex);

    return pid;
}

static void RemoveFromProcessTable(pid_t pid) {
    AcquireMutex(process_table_mutex, -1);

    struct process_table_node dummy;
    dummy.pid = pid;
    struct process_table_node* actual = AvlTreeGet(process_table, (void*) &dummy);
    AvlTreeDelete(process_table, (void*) actual);
    FreeHeap(actual);       // this was allocated on 'InsertIntoProcessTable'

    ReleaseMutex(process_table_mutex);
}

struct process* PAGEABLE_CODE_SECTION GetProcessFromPid(pid_t pid) {
    MAX_IRQL(IRQL_STANDARD);

    AcquireMutex(process_table_mutex, -1);

    struct process_table_node dummy;
    dummy.pid = pid;
    struct process_table_node* node = AvlTreeGet(process_table, (void*) &dummy);

    ReleaseMutex(process_table_mutex);

    return node->process;
}

void LockProcess(struct process* prcss) {
    AcquireMutex(prcss->lock, -1);
}

void UnlockProcess(struct process* prcss) {
    ReleaseMutex(prcss->lock);
}

void InitProcess(void) {
    InitSpinlock(&pid_lock, "pid", IRQL_SCHEDULER);
    process_table_mutex = CreateMutex();
    LogWriteSerial("prcsstablemutex = 0x%X\n", process_table_mutex);
    process_table = AvlTreeCreate();
    AvlTreeSetComparator(process_table, ProcessTableComparator);
}

struct process* PAGEABLE_CODE_SECTION CreateProcess(pid_t parent_pid) {
    MAX_IRQL(IRQL_STANDARD);

    struct process* prcss = AllocHeap(sizeof(struct process));

    prcss->lock = CreateMutex();
    prcss->vas = CreateVas();
    prcss->parent = parent_pid;
    prcss->children = AvlTreeCreate();
    prcss->threads = AvlTreeCreate();
    prcss->killed_children_semaphore = CreateSemaphore(1000, 1000);
    prcss->retv = 0;
    prcss->terminated = false;
    prcss->pid = InsertIntoProcessTable(prcss);

    if (parent_pid != 0) {
        struct process* parent = GetProcessFromPid(parent_pid);
        LockProcess(parent);
        AvlTreeInsert(parent->children, (void*) prcss);
        UnlockProcess(parent);
    }

    return prcss;
}

void AddThreadToProcess(struct process* prcss, struct thread* thr) {
    LockProcess(prcss);
    AvlTreeInsert(prcss->threads, (void*) thr);
    thr->process = prcss;
    UnlockProcess(prcss);
}

struct process* PAGEABLE_CODE_SECTION ForkProcess(void) {
    EXACT_IRQL(IRQL_STANDARD);

    LockProcess(GetProcess());

    struct process* new_process = CreateProcess(GetProcess()->pid);
    DestroyVas(new_process->vas);

    // TODO: there are probably more things to copy over in the future (e.g. list of open file descriptors, etc.)
    //       the open files, etc.

    new_process->vas = CopyVas();
    //TODO: need to grab the first thread (I don't think we've ordered threads by thread id yet in the AVL)
    //      so will need to fix that first.
    //CopyThreadToNewProcess(new_process, )
    UnlockProcess(GetProcess());

    return new_process;
}

static void PAGEABLE_CODE_SECTION ReapProcess(struct process* prcss) {
    EXACT_IRQL(IRQL_STANDARD);

    // TOOD: there's more cleanup to be done here..., e.g. VAS()

    // TODO: need to destroy semaphore, but of course it started at some huge number (e.g. 1 << 30), 
    //       so destroy is going to shit itself. probably need to pass flags to CreateSemaphore to have
    //       an 'inverse' mode - or check that on deletion it equals what it started on, or just an IGNORE_HELD_ON_DELETE?

    RemoveFromProcessTable(prcss->pid);
    if (prcss->parent != 0) {
        struct process* parent = GetProcessFromPid(prcss->parent);
        AvlTreeDelete(parent->children, prcss);
    }
    FreeHeap(prcss);
}

static pid_t PAGEABLE_CODE_SECTION TryReapProcessAux(struct process* parent, struct avl_node* node, pid_t target, int* status) {
    EXACT_IRQL(IRQL_STANDARD);
    
    if (node == NULL) {
        return 0;
    }

    struct process* child = (struct process*) AvlTreeGetData(node);

    LockProcess(child);

    if (child->terminated && (child->pid == target || target == (pid_t) -1)) {
        *status = child->retv;
        pid_t pid = child->pid;
        UnlockProcess(child);       // needed in case someone is waiting on us, before our death
        ReapProcess(child);
        return pid;
    }

    UnlockProcess(child);

    pid_t left_retv = TryReapProcessAux(parent, AvlTreeGetLeft(node), target, status);
    if (left_retv != 0) {
        return left_retv;
    }
    return TryReapProcessAux(parent, AvlTreeGetRight(node), target, status);
}

static pid_t PAGEABLE_CODE_SECTION TryReapProcess(struct process* parent, pid_t target, int* status) {
    EXACT_IRQL(IRQL_STANDARD);
    return TryReapProcessAux(parent, AvlTreeGetRootNode(parent->children), target, status);
} 

pid_t PAGEABLE_CODE_SECTION WaitProcess(pid_t pid, int* status, int flags) {
    EXACT_IRQL(IRQL_STANDARD);

    (void) flags;

    struct process* prcss = GetProcess();
    
    pid_t result = 0;
    int failed_reaps = 0;
    while (result == 0) {
        AcquireSemaphore(prcss->killed_children_semaphore, -1);
        LockProcess(prcss);
        result = TryReapProcess(prcss, pid, status);
        UnlockProcess(prcss);
        if (result == 0 && pid != (pid_t) -1) {
            failed_reaps++;
        }
    }

    /*
     * Ensure that the next time we call WaitProcess(), we can immediately retry the reaps that
     * we increased the semaphore for, but didn't actually reap on.
     */
    while (failed_reaps--) {
        ReleaseSemaphore(prcss->killed_children_semaphore);
    }

    return result;
}

/**
 * Changes the parent of a parentless process. Used to ensure the initial process can always reap orphaned
 * processes.
 */
static void PAGEABLE_CODE_SECTION AdoptOrphan(struct process* adopter, struct process* ophan) {
    EXACT_IRQL(IRQL_STANDARD);

    LockProcess(adopter);

    ophan->parent = adopter->pid;
    AvlTreeInsert(adopter->children, (void*) ophan);
    ReleaseSemaphore(adopter->killed_children_semaphore);

    UnlockProcess(adopter);
}

/**
 * Recursively converts all child processes in a process' thread tree into zombie processes.
 * 
 * @param node The subtree to start from. NULL is acceptable, and is the recursion base case.
 * @note EXACT_IRQL(IRQL_STANDARD)
 */
static void PAGEABLE_CODE_SECTION OrphanChildProcesses(struct avl_node* node) {
    EXACT_IRQL(IRQL_STANDARD);

    if (node == NULL) {
        return;
    }

    OrphanChildProcesses(AvlTreeGetLeft(node));
    OrphanChildProcesses(AvlTreeGetRight(node));

    AdoptOrphan(GetProcessFromPid(1), AvlTreeGetData(node));
}

/**
 * Recursively terminates all threads in a process' thread tree.
 * 
 * @param node The subtree to start from. NULL is acceptable, and is the recursion base case.
 * @note EXACT_IRQL(IRQL_STANDARD)
 */
static void PAGEABLE_CODE_SECTION KillRemainingThreads(struct avl_node* node) {
    EXACT_IRQL(IRQL_STANDARD);

    if (node == NULL) {
        return;
    }

    KillRemainingThreads(AvlTreeGetLeft(node));
    KillRemainingThreads(AvlTreeGetRight(node));

    struct thread* victim = (struct thread*) AvlTreeGetData(node);

    /*
     * We have already called terminate on the calling thread within `KillProcess`, 
     * so no need to do it again.
     */
    assert(!victim->death_sentence);
    if (victim->state != THREAD_STATE_TERMINATED) {
        TerminateThread(victim);
    }
}

/**
 * Does all of the required operations to kill a process. This is run in its own thread, without an owning
 * process, so that a process doesn't try to delete itself (and therefore delete its stack).
 * 
 * @param arg The process to kill (needs to be cast to struct process*)
 * @note EXACT_IRQL(IRQL_STANDARD)
 */
static void PAGEABLE_CODE_SECTION KillProcessHelper(void* arg) {
    EXACT_IRQL(IRQL_STANDARD);

    struct process* prcss = arg;

    assert(GetProcess() == NULL);
    assert(GetVas() != prcss->vas);     // we should be on GetKernelVas()

    KillRemainingThreads(AvlTreeGetRootNode(prcss->threads));
    AvlTreeDestroy(prcss->threads);
    
    OrphanChildProcesses(AvlTreeGetRootNode(prcss->children));
    AvlTreeDestroy(prcss->children);

    DestroyVas(prcss->vas);

    prcss->terminated = true;

    if (prcss->parent == 0) {
        ReapProcess(prcss);

    } else {
        struct process* parent = GetProcessFromPid(prcss->parent);
        ReleaseSemaphore(parent->killed_children_semaphore);
    }

    TerminateThread(GetThread());
}

/**
 * Deletes the process holding the thread currently running on this CPU, and all threads within that process.
 * If the process being deleted has child processes that still exist, their parent will change to the process with 
 * pid 1. If the process being deleted has a parent, then it becomes a zombie process until the parent reaps it
 * by calling `WaitProcess`. If the process being deleted has no parent, it will be reaped and deallocated immediately.
 * 
 * This function does not return.
 * 
 * @param retv The return value the process being deleted will give.
 * 
 * @note MAX_IRQL(IRQL_STANDARD)
 */
void PAGEABLE_CODE_SECTION KillProcess(int retv) {
    MAX_IRQL(IRQL_STANDARD);

    struct process* prcss = GetProcess();
    prcss->retv = retv;
    
    /**
     * Must run it in a different thread and process (a NULL process is fine), as it is going to kill all
     * threads in the process, and the process itself. Obviously, this means we can't be running on said
     * threads or process. 
     */
    CreateThreadEx(KillProcessHelper, (void*) prcss, GetKernelVas(), "process killer", NULL, SCHEDULE_POLICY_FIXED, FIXED_PRIORITY_KERNEL_HIGH);

    TerminateThread(GetThread());
}

/**
 * Returns a pointer to the process that the thread currently running on this CPU belongs to. If there is no
 * running thread (i.e. multitasking hasn't started yet), or the thread does not belong to a process, NULL
 * is returned.
 * 
 * @return The process of the current thread, if it exists, or NULL otherwise.
 * 
 * @note MAX_IRQL(IRQL_HIGH) 
 */
struct process* GetProcess(void) {
    MAX_IRQL(IRQL_HIGH);
    
    struct thread* thr = GetThread();
    if (thr == NULL) {
        return NULL;
    }
    return thr->process;
}

/**
 * Creates a new process and an initial thread within the process.
 * 
 * @param parent        The process id (pid) of the process which will become the parent of the newly created process.
 *                          To create process without a parent, set to zero.
 * @param entry_point   The address to a function where the thread will begin execution from. `args` will be passed into
 *                          this function as an argument.
 * @param args          The argument to call the entry_point function with when the thread starts executing
 * 
 * @return The newly created process
 * 
 * @note MAX_IRQL(IRQL_STANDARD)
 */
struct process* CreateProcessWithEntryPoint(pid_t parent, void(*entry_point)(void*), void* args) {
    MAX_IRQL(IRQL_STANDARD);
    struct process* prcss = CreateProcess(parent);
    struct thread* thr = CreateThread(entry_point, args, prcss->vas, "init");
    AddThreadToProcess(prcss, thr);
    return prcss;
}

/**
 * Returns the process id (pid) of a given process.
 * 
 * @param prcss The process to get the pid of
 * @return The process id
 * 
 * @note MAX_IRQL(IRQL_HIGH) 
 */
pid_t GetPid(struct process* prcss) {
    MAX_IRQL(IRQL_HIGH);
    return prcss->pid;
}