
#include <heap.h>
#include <stdlib.h>
#include <vfs.h>
#include <log.h>
#include <assert.h>
#include <irql.h>
#include <errno.h>
#include <string.h>
#include <transfer.h>
#include <sys/stat.h>
#include <dirent.h>
#include <panic.h>
#include <thread.h>
#include <termios.h>
#include <blockingbuffer.h>
#include <virtual.h>

#define INTERNAL_BUFFER_SIZE 256        // used to communicate with master and sub. can have any length - but lower means both input AND **PRINTING** will incur more semaphore trashing
#define LINE_BUFFER_SIZE 300            // maximum length of a typed line
#define FLUSHED_BUFFER_SIZE 500         // used to store any leftover after pressing '\n' that the program has yet to read

struct pty_master_internal_data {
    struct vnode* subordinate;
    struct blocking_buffer* display_buffer;
    struct blocking_buffer* keybrd_buffer;
    struct blocking_buffer* flushed_buffer;
    struct thread* line_processing_thread;
};

struct pty_subordinate_internal_data {
    struct vnode* master;
    struct termios termios;
    char line_buffer[LINE_BUFFER_SIZE];
    uint8_t line_buffer_char_width[LINE_BUFFER_SIZE];
    int line_buffer_pos;
};

// "THE SCREEN"
static int MasterRead(struct vnode* node, struct transfer* tr) {  
    struct pty_master_internal_data* internal = node->data;
    while (tr->length_remaining > 0) {
        char c = BlockingBufferGet(internal->display_buffer);
        PerformTransfer(&c, tr, 1);
    }

    return 0;
}

// "THE KEYBOARD"
static int MasterWrite(struct vnode* node, struct transfer* tr) {
    struct pty_master_internal_data* internal = node->data;

    while (tr->length_remaining > 0) {
        char c;
        PerformTransfer(&c, tr, 1);
        BlockingBufferAdd(internal->keybrd_buffer, c, true);
    }

    return 0;
}

static int MasterWait(struct vnode*, int, uint64_t) {
    return ENOSYS;
}

static int SubordinateWait(struct vnode*, int, uint64_t) {
    return ENOSYS;
}

static void FlushSubordinateLineBuffer(struct vnode* node) {
    struct pty_subordinate_internal_data* internal = node->data;
    struct pty_master_internal_data* master_internal = internal->master->data;

    // could add a 'BlockingBufferAddMany' call?
    for (int i = 0; i < internal->line_buffer_pos; ++i) {
        BlockingBufferAdd(master_internal->flushed_buffer, internal->line_buffer[i], true);
    }

    internal->line_buffer_pos = 0;
}

static void RemoveFromSubordinateLineBuffer(struct vnode* node) {
    struct pty_subordinate_internal_data* internal = node->data;

    if (internal->line_buffer_pos == 0) {
        return;
    }

    internal->line_buffer[--internal->line_buffer_pos] = 0;
}

static void AddToSubordinateLineBuffer(struct vnode* node, char c, int width) {
    struct pty_subordinate_internal_data* internal = node->data;

    if (internal->line_buffer_pos == LINE_BUFFER_SIZE) {
        Panic(PANIC_NOT_IMPLEMENTED);
        return;
    }

    internal->line_buffer[internal->line_buffer_pos] = c;
    internal->line_buffer_char_width[internal->line_buffer_pos] = width;
    internal->line_buffer_pos++;
}

static void LineProcessor(void* sub_) {
    SetThreadPriority(GetThread(), SCHEDULE_POLICY_FIXED, FIXED_PRIORITY_KERNEL_HIGH);

    struct vnode* node = (struct vnode*) sub_;
    struct pty_subordinate_internal_data* internal = node->data;
    struct pty_master_internal_data* master_internal = internal->master->data;

    while (true) {
        bool echo = internal->termios.c_lflag & ECHO;
        bool canon = internal->termios.c_lflag & ICANON;

        char c = BlockingBufferGet(master_internal->keybrd_buffer);

        /*
         * This must happen before we modify the line buffer (i.e. to add or backspace a character), as
         * the backspace code here needs to check for a non-empty line (and so this must be done before we make 
         * the line empty).
         */
        if (echo) {
            if (c == '\b' && canon) {
                if (internal->line_buffer_pos > 0) {
                    BlockingBufferAdd(master_internal->display_buffer, '\b', true);
                    BlockingBufferAdd(master_internal->display_buffer, ' ', true);
                    BlockingBufferAdd(master_internal->display_buffer, '\b', true);
                }
            } else {
                BlockingBufferAdd(master_internal->display_buffer, c, true);
            }
        }

        if (c == '\b' && canon) {
            RemoveFromSubordinateLineBuffer(node);

        } else {
            AddToSubordinateLineBuffer(node, c, 1);
        }

        if (c == '\n' || c == 3 || !canon) {
            FlushSubordinateLineBuffer(node);
        }
    }
}

// "THE STDIN LINE BUFFER"
static int SubordinateRead(struct vnode* node, struct transfer* tr) {        
    struct pty_subordinate_internal_data* internal = (struct pty_subordinate_internal_data*) node->data;
    struct pty_master_internal_data* master_internal = (struct pty_master_internal_data*) internal->master->data;

    if (tr->length_remaining == 0) {
        return 0;
    }

    char c = BlockingBufferGet(master_internal->flushed_buffer);
    PerformTransfer(&c, tr, 1);

    int res = 0;
    while (tr->length_remaining > 0 && !(res = BlockingBufferTryGet(master_internal->flushed_buffer, (uint8_t*) &c))) {
        PerformTransfer(&c, tr, 1);
    }

    return 0;
}

// "WRITING TO STDOUT"
static int SubordinateWrite(struct vnode* node, struct transfer* tr) {
    struct pty_subordinate_internal_data* internal = (struct pty_subordinate_internal_data*) node->data;
    struct pty_master_internal_data* master_internal = (struct pty_master_internal_data*) internal->master->data;

    while (tr->length_remaining > 0) {
        char c;
        int err = PerformTransfer(&c, tr, 1);
        if (err) {
            return err;
        }

        BlockingBufferAdd(master_internal->display_buffer, c, true);
    }
    
    return 0;
}

static int GenericStat(struct vnode*, struct stat* st) {
    st->st_mode = S_IFCHR | S_IRWXU | S_IRWXG | S_IRWXO;
    st->st_atime = 0;
    st->st_blksize = 0;
    st->st_blocks = 0;
    st->st_ctime = 0;
    st->st_dev = 0xBABECAFE;
    st->st_gid = 0;
    st->st_ino = 0xCAFEBABE;
    st->st_mtime = 0;
    st->st_nlink = 1;
    st->st_rdev = 0xCAFEDEAD;
    st->st_size = 0;
    st->st_uid = 0;
    return 0;
}

static int SubordinateCheckTty(struct vnode*) {
    return 0;
}

static const struct vnode_operations master_operations = {
    .read           = MasterRead,
    .write          = MasterWrite,
    .stat           = GenericStat,
    .wait           = MasterWait,
};

static const struct vnode_operations subordinate_operations = {
    .check_tty      = SubordinateCheckTty,
    .read           = SubordinateRead,
    .write          = SubordinateWrite,
    .stat           = GenericStat,
    .wait           = SubordinateWait,
};

void CreatePseudoTerminal(struct vnode** master, struct vnode** subordinate) {
    struct vnode* m = CreateVnode(master_operations);
    struct vnode* s = CreateVnode(subordinate_operations);
    
    struct pty_master_internal_data* m_data = AllocHeap(sizeof(struct pty_master_internal_data));
    struct pty_subordinate_internal_data* s_data = AllocHeap(sizeof(struct pty_subordinate_internal_data));

    m_data->subordinate = s;
    m_data->display_buffer = BlockingBufferCreate(INTERNAL_BUFFER_SIZE);
    m_data->keybrd_buffer = BlockingBufferCreate(INTERNAL_BUFFER_SIZE);
    m_data->flushed_buffer = BlockingBufferCreate(FLUSHED_BUFFER_SIZE);
    m_data->line_processing_thread = CreateThread(LineProcessor, (void*) s, GetVas(), "line processor");

    s_data->master = m;
    s_data->termios.c_lflag = ICANON | ECHO;

    m->data = m_data;
    s->data = s_data;
    *master = m;
    *subordinate = s;
}
