
/*
 * thread/process.c - Processes
 */

#include <arch.h>
#include <irql.h>
#include <thread.h>
#include <assert.h>
#include <virtual.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <avl.h>
#include <panic.h>
#include <semaphore.h>
#include <filedes.h>
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
    struct filedes_table* filedes_table;
    int retv;
    bool terminated;
};

struct process_table_node {
    pid_t pid;
    struct process* process;
};

static struct spinlock pid_lock;
static struct avl_tree* process_table;
static struct semaphore* process_table_mutex;

static int ProcessTableComparator(void* a_, void* b_) {
    struct process_table_node* a = a_;
    struct process_table_node* b = b_;
    return COMPARE_SIGN(a->pid, b->pid);
}

static pid_t AllocateNextPid(void) {
    static pid_t next_pid = 1;

    AcquireSpinlock(&pid_lock);
    pid_t pid = next_pid++;
    ReleaseSpinlock(&pid_lock);

    return pid;
}

static int InsertIntoProcessTable(struct process* prcss) {
    pid_t pid = AllocateNextPid();

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

    struct process_table_node dummy = {.pid = pid};
    struct process_table_node* actual = AvlTreeGet(process_table, (void*) &dummy);
    AvlTreeDelete(process_table, (void*) actual);
    FreeHeap(actual);       // this was allocated on 'InsertIntoProcessTable'

    ReleaseMutex(process_table_mutex);
}

void LockProcess(struct process* prcss) {
    AcquireMutex(prcss->lock, -1);
}

void UnlockProcess(struct process* prcss) {
    ReleaseMutex(prcss->lock);
}

void InitProcess(void) {
    InitSpinlock(&pid_lock, "pid", IRQL_SCHEDULER);
    process_table_mutex = CreateMutex("prcss table");
    process_table = AvlTreeCreate();
    AvlTreeSetComparator(process_table, ProcessTableComparator);
}

struct process* CreateProcess(pid_t parent_pid) {
    EXACT_IRQL(IRQL_STANDARD);   

    struct process* prcss = AllocHeap(sizeof(struct process));

    prcss->lock = CreateMutex("prcss");
    prcss->vas = CreateVas();
    prcss->parent = parent_pid;
    prcss->children = AvlTreeCreate();
    prcss->threads = AvlTreeCreate();
    prcss->killed_children_semaphore = CreateSemaphore("killed children", SEM_BIG_NUMBER, SEM_BIG_NUMBER);
    prcss->retv = 0;
    prcss->terminated = false;
    prcss->pid = InsertIntoProcessTable(prcss);
    prcss->filedes_table = CreateFileDescriptorTable();

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

struct process* ForkProcess(void) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    LockProcess(GetProcess());

    struct process* new_process = CreateProcess(GetProcess()->pid);
    DestroyVas(new_process->vas);

    // TODO: there are probably more things to copy over in the future (e.g. list of open file descriptors, etc.)
    //       the open files, etc.


    // TODO: file descriptor table...

    new_process->vas = CopyVas();
    //TODO: need to grab the first thread (I don't think we've ordered threads by thread id yet in the AVL)
    //      so will need to fix that first.
    //CopyThreadToNewProcess(new_process, )
    UnlockProcess(GetProcess());

    return new_process;
}

/**
 * Directly reaps a process.
 */
static void ReapProcess(struct process* prcss) {
    // TOOD: there's more cleanup to be done here... ?
    assert(prcss->vas != GetVas());
    
    int res = DestroySemaphore(prcss->killed_children_semaphore, SEM_REQUIRE_FULL);
    (void) res;
    assert(res == 0);
    DestroyVas(prcss->vas);
    DestroyFileDescriptorTable(prcss->filedes_table);
    RemoveFromProcessTable(prcss->pid);
    if (prcss->parent != 0) {
        struct process* parent = GetProcessFromPid(prcss->parent);
        AvlTreeDelete(parent->children, prcss);
    }
    FreeHeap(prcss);
}

/**
 * Recursively goes through the children of a process, reaping the first child that is able to be reaped.
 * Depending on the value of `target`, it will either reap the first potential candidate, or a particular candidate.
 *
 * @param parent The process whose children we are looking through
 * @param node The current subtree of the parent process' children tree
 * @param target Either the process ID of the child to reap, or -1 to reap the first valid candidate.
 * @param status If a child is reaped, its return value will be written here.
 * @return The process ID of the reaped child, or 0 if no children are reaped.
 */
static pid_t RecursivelyTryReap(struct process* parent, struct avl_node* node, pid_t target, int* status) {    
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

    pid_t left_retv = RecursivelyTryReap(parent, AvlTreeGetLeft(node), target, status);
    if (left_retv != 0) {
        return left_retv;
    }
    return RecursivelyTryReap(parent, AvlTreeGetRight(node), target, status);
}


/**
 * Changes the parent of a parentless process. Used to ensure the initial process can always reap orphaned
 * processes.
 */
static void AdoptOrphan(struct process* adopter, struct process* ophan) {
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
 */
static void RecursivelyMakeChildrenOrphans(struct avl_node* node) {
    if (node == NULL) {
        return;
    }
    
    RecursivelyMakeChildrenOrphans(AvlTreeGetLeft(node));
    RecursivelyMakeChildrenOrphans(AvlTreeGetRight(node));
    AdoptOrphan(GetProcessFromPid(1), AvlTreeGetData(node));
}

/**
 * Recursively terminates all threads in a process' thread tree.
 * 
 * @param node The subtree to start from. NULL is acceptable, and is the recursion base case.
 */
static void RecursivelyKillRemainingThreads(struct avl_node* node) {
    if (node == NULL) {
        return;
    }

    RecursivelyKillRemainingThreads(AvlTreeGetLeft(node));
    RecursivelyKillRemainingThreads(AvlTreeGetRight(node));

    struct thread* victim = AvlTreeGetData(node);
    if (victim->state != THREAD_STATE_TERMINATED && !victim->needs_termination) {
        TerminateThread(victim);
    }
}

/**
 * Does all of the required operations to kill a process. This is run in its own thread, without an owning
 * process, so that a process doesn't try to delete itself (and therefore delete its stack).
 * 
 * @param arg The process to kill (needs to be cast to struct process*)
 */
static void KillProcessHelper(void* arg) {
    struct process* prcss = arg;

    assert(GetProcess() == NULL);
    assert(GetVas() != prcss->vas);     // we should be on GetKernelVas()

    RecursivelyKillRemainingThreads(AvlTreeGetRootNode(prcss->threads));    
    RecursivelyMakeChildrenOrphans(AvlTreeGetRootNode(prcss->children));

    AvlTreeDestroy(prcss->threads);
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
 * Deletes the current process and all its threads. Child processes have their parent switched to pid 1.
 * If the process being deleted has a parent, then it becomes a zombie process until the parent reaps it
 * If the process being deleted has no parent, it will be reaped and deallocated immediately.
 * 
 * This function does not return.
 * 
 * @param retv The return value the process being deleted will give.
 */
void KillProcess(int retv) {
    MAX_IRQL(IRQL_STANDARD);   

    struct process* prcss = GetProcess();
    prcss->retv = retv;
    
    /**
     * Must run it in a different thread and process (a NULL process is fine), as it is going to kill all
     * threads in the process, and the process itself.
     */
    CreateThreadEx(KillProcessHelper, (void*) prcss, GetKernelVas(), "process killer", NULL, 
        SCHEDULE_POLICY_FIXED, FIXED_PRIORITY_KERNEL_HIGH, 0);

    TerminateThread(GetThread());
}

/**
 * Returns a pointer to the process that the thread currently running on this CPU belongs to. If there is no
 * running thread (i.e. multitasking hasn't started yet), or the thread does not belong to a process, NULL
 * is returned.
 * 
 * @return The process of the current thread, if it exists, or NULL otherwise.
 */
struct process* GetProcess(void) {
    MAX_IRQL(IRQL_HIGH);
    struct thread* thr = GetThread();
    return thr == NULL ? NULL : thr->process;
}

struct process* CreateProcessWithEntryPoint(pid_t parent, void(*entry_point)(void*), void* args) {
    EXACT_IRQL(IRQL_STANDARD);   
    struct process* prcss = CreateProcess(parent);
    struct thread* thr = CreateThread(entry_point, args, prcss->vas, "prcssinit");
    AddThreadToProcess(prcss, thr);
    return prcss;
}

/**
 * Returns the file descriptor table of the given process. Returns NULL if 
 * `prcss` is null.
 */
struct filedes_table* GetFileDescriptorTable(struct process* prcss) {
    if (prcss == NULL) {
        return NULL;
    }

    return prcss->filedes_table;
}

/** 
 * Given a process id, returns the process object. Returns NULL for an invalid
 * `pid`.
 */
struct process* GetProcessFromPid(pid_t pid) {
    EXACT_IRQL(IRQL_STANDARD);

    AcquireMutex(process_table_mutex, -1);

    struct process_table_node dummy = {.pid = pid};
    struct process_table_node* node = AvlTreeGet(process_table, (void*) &dummy);

    ReleaseMutex(process_table_mutex);

    return node == NULL ? NULL : node->process;
}

/**
 * Returns the process id of a given process. If `prcss` is null, 0 is returned.
 */
pid_t GetPid(struct process* prcss) {
    MAX_IRQL(IRQL_HIGH);
    return prcss == NULL ? 0 : prcss->pid;
}

pid_t WaitProcess(pid_t pid, int* status, int flags) {
    EXACT_IRQL(IRQL_STANDARD);   

    struct process* prcss = GetProcess();
    
    pid_t result = 0;
    int failed_reaps = 0;
    while (result == 0) {
        int res = AcquireSemaphore(prcss->killed_children_semaphore, (flags & WNOHANG) ? 0 : -1);
        if (res != 0) {
            break;
        }
        LockProcess(prcss);
        result = RecursivelyTryReap(prcss, AvlTreeGetRootNode(prcss->children), pid, status);
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