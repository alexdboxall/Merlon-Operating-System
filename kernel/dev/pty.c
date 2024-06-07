
/*
 * dev/pipe.c - Unnamed Pipes
 *
 * Implements pseudoterminals (pty) - a pair of devices which act as a virtual
 * terminal, with the master implementing the terminal emulator, and the
 * subordinate acting as the text terminal.
 */

#include <heap.h>
#include <stdlib.h>
#include <vfs.h>
#include <tree.h>
#include <video.h>
#include <log.h>
#include <process.h>
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
#include <mailbox.h>
#include <virtual.h> 
#include <sys/ioctl.h>
#include <ksignal.h>
#include <signal.h>
#include <sys/types.h>

#define CTRL_C ('C' - 64)
#define CTRL_Q ('Q' - 64)
#define CTRL_Z ('Z' - 64)

#define INTERNAL_BUFFER_SIZE 4096   // used to communicate with master and sub   
                                    // used for displaying, so larger buffer
                                    // means text prints faster
#define LINE_BUFFER_SIZE 300        // maximum length of a typed line
#define FLUSHED_BUFFER_SIZE 300     // used to store any leftover after pressing '\n'

struct master_data {
    struct vnode* subordinate;
    struct mailbox* display_buffer;
    struct mailbox* keybrd_buffer;
    struct mailbox* flushed_buffer;
    struct thread* line_processing_thread;
};

struct sub_data {
    struct vnode* master;
    struct termios termios;
    char line_buffer[LINE_BUFFER_SIZE];
    uint8_t line_buffer_char_width[LINE_BUFFER_SIZE];
    int line_buffer_pos;
    pid_t controlling_pgid;
};

static void FlushSubordinateLineBuffer(struct vnode* node) {
    struct sub_data* internal = node->data;
    struct master_data* master_internal = internal->master->data;

    for (int i = 0; i < internal->line_buffer_pos; ++i) {
        MailboxAdd(master_internal->flushed_buffer, -1, internal->line_buffer[i]);
    }
    internal->line_buffer_pos = 0;
}

static void RemoveFromSubordinateLineBuffer(struct vnode* node) {
    struct sub_data* internal = node->data;

    if (internal->line_buffer_pos == 0) {
        return;
    }

    internal->line_buffer[--internal->line_buffer_pos] = 0;
}

static void AddToSubordinateLineBuffer(struct vnode* node, char c, int width) {
    struct sub_data* internal = node->data;

    if (internal->line_buffer_pos == LINE_BUFFER_SIZE) {
        return;
    }

    internal->line_buffer[internal->line_buffer_pos] = c;
    internal->line_buffer_char_width[internal->line_buffer_pos] = width;
    internal->line_buffer_pos++;
}

static void LineProcessor(void* sub_) {
    //SetThreadPriority(GetThread(), SCHEDULE_POLICY_FIXED, FIXED_PRIORITY_KERNEL_HIGH);

    struct vnode* node = (struct vnode*) sub_;
    struct sub_data* internal = node->data;
    struct master_data* master_internal = internal->master->data;

    while (true) {
        bool echo = internal->termios.c_lflag & ECHO;
        bool canon = internal->termios.c_lflag & ICANON;

        uint8_t c;
        MailboxGet(master_internal->keybrd_buffer, -1, &c);

        LogWriteSerial("Got character: %d\n", c);

        /*
         * Must happen before we modify the line buffer (i.e. to add / backspace 
         * a character), as the backspace code needs to check for non-empty 
         * lines (so this must be done before we make the line empty).
         */
        if (echo) {
            if (c == '\b' && canon) {
                if (internal->line_buffer_pos > 0) {
                    MailboxAdd(master_internal->display_buffer, -1, '\b');
                    MailboxAdd(master_internal->display_buffer, -1, ' ');
                    MailboxAdd(master_internal->display_buffer, -1, '\b');
                }
            } else {
                MailboxAdd(master_internal->display_buffer, -1, c);
            }
        }

        if (c == '\b' && canon) {
            RemoveFromSubordinateLineBuffer(node);

        } else {
            AddToSubordinateLineBuffer(node, c, 1);
        }

        /*
         * ASCII 3 is `CTRL+C`, and so we want to handle that straight away so
         * we can send SIGINT. 
         */
        if (c == '\n' || c == CTRL_C || c == CTRL_Q || c == CTRL_Z || !canon) {
            if (c == CTRL_C) {
                RaiseSignalToProcessGroup(internal->controlling_pgid, SIGINT);
            } else if (c == CTRL_Q) {
                RaiseSignalToProcessGroup(internal->controlling_pgid, SIGCONT);
            } else if (c == CTRL_Z) {
                RaiseSignalToProcessGroup(internal->controlling_pgid, SIGSTOP);
            }
            FlushSubordinateLineBuffer(node);
        }
    }
}


// "THE SCREEN"
static int MasterRead(struct vnode* node, struct transfer* tr) {  
    struct master_data* internal = node->data;
    return MailboxAccess(internal->display_buffer, tr);
}

// "THE KEYBOARD"
static int MasterWrite(struct vnode* node, struct transfer* tr) {
    struct master_data* internal = node->data;
    return MailboxAccess(internal->keybrd_buffer, tr);
}

// "THE STDIN LINE BUFFER"
static int SubordinateRead(struct vnode* node, struct transfer* tr) {        
    struct sub_data* internal = (struct sub_data*) node->data;
    struct master_data* master_internal = (struct master_data*) internal->master->data;
    return MailboxAccess(master_internal->flushed_buffer, tr);
}

// "WRITING TO STDOUT"
static int SubordinateWrite(struct vnode* node, struct transfer* tr) {
    struct sub_data* internal = (struct sub_data*) node->data;
    struct master_data* master_internal = (struct master_data*) internal->master->data;
    int res = 0;
    while (tr->length_remaining > 0 && (res == 0 || res == EINTR)) {
        res = MailboxAccess(master_internal->display_buffer, tr);
    }
    return res;
}

static int MasterClose(struct vnode* node) {
    struct master_data* internal = (struct master_data*) node->data;
    MailboxDestroy(internal->display_buffer);
    MailboxDestroy(internal->flushed_buffer);
    MailboxDestroy(internal->keybrd_buffer);
    TerminateThread(internal->line_processing_thread);
    FreeHeap(internal);
    return 0;
}

static int SubordinateIoctl(struct vnode* node, int cmd, void* arg) {
    struct sub_data* internal = (struct sub_data*) node->data;

    if (cmd == TCSETS || cmd == TCSETSW || cmd == TCSETSF) {
        if (cmd != TCSETS) {
            return ENOSYS;
        }
        
        struct termios new_term;
        struct transfer tr = CreateTransferReadingFromUser(arg, sizeof(struct termios), 0);
        int res = PerformTransfer(&new_term, &tr, sizeof(struct termios));
        if (res != 0) {
            return res;
        }

        // TODO: VALIDATE THE CONTENTS OF NEW_TERM

        internal->termios = new_term;
        return 0;

    } else if (cmd == TCGETS) {
        struct transfer tr = CreateTransferWritingToUser(arg, sizeof(struct termios), 0);
        return PerformTransfer(&internal->termios, &tr, sizeof(struct termios));

    } else if (cmd == TIOCGPGRP) {
        struct transfer tr = CreateTransferWritingToUser(arg, sizeof(pid_t), 0);
        return PerformTransfer(&internal->controlling_pgid, &tr, sizeof(pid_t));

    } else if (cmd == TIOCSPGRP) {
        pid_t new_pgid;
        struct transfer tr = CreateTransferReadingFromUser(arg, sizeof(pid_t), 0);
        int res = PerformTransfer(&new_pgid, &tr, sizeof(pid_t));
        if (res != 0) {
            return res;
        }
        LogWriteSerial("setting the termial controlling pgid to %d\n", new_pgid);
        internal->controlling_pgid = new_pgid;
        return 0;
    
    } else {
        return EINVAL;
    }
}

static const struct vnode_operations master_operations = {
    .read           = MasterRead,
    .write          = MasterWrite,
    .close          = MasterClose,
};

static const struct vnode_operations subordinate_operations = {
    .read           = SubordinateRead,
    .write          = SubordinateWrite,
    .ioctl          = SubordinateIoctl,
};

void CreatePseudoTerminal(struct vnode** master, struct vnode** subordinate) {
    struct stat st = (struct stat) {
        .st_mode =  S_IFCHR | S_IRWXU | S_IRWXG | S_IRWXO,
        .st_nlink = 1,
        .st_dev = NextDevId()
    };
    struct vnode* m = CreateVnode(master_operations, st);
    struct vnode* s = CreateVnode(subordinate_operations, st);
    
    struct master_data* m_data = AllocHeap(sizeof(struct master_data));
    struct sub_data* s_data = AllocHeap(sizeof(struct sub_data));

    m_data->subordinate = s;
    m_data->display_buffer = MailboxCreate(INTERNAL_BUFFER_SIZE);
    m_data->keybrd_buffer = MailboxCreate(INTERNAL_BUFFER_SIZE);
    m_data->flushed_buffer = MailboxCreate(FLUSHED_BUFFER_SIZE);
    m_data->line_processing_thread = CreateThread(LineProcessor, (void*) s, GetVas(), "ptty");

    s_data->master = m;
    s_data->termios.c_lflag = ICANON | ECHO;
    s_data->controlling_pgid = GetPid(GetProcess());

    m->data = m_data;
    s->data = s_data;
    *master = m;
    *subordinate = s;
}
