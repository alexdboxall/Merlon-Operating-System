
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

struct atexit_handler {
    union {
        void (*standard)(void);
        void* arg;
    };
    void (*advanced)(int, void*);
};

static struct atexit_handler handlers[ATEXIT_MAX];
static int num_handlers = 0;


int atexit(void (*function)(void)) {
    // TODO: lock
    if (num_handlers >= ATEXIT_MAX) {
        return 1;
    }

    handlers[num_handlers++] = (struct atexit_handler) {
        .standard = function,
        .advanced = NULL,
    };

    // TODO: unlock

    return 0;
}

int on_exit(void (*function)(int, void*), void *arg) {
    // TODO: lock
    if (num_handlers >= ATEXIT_MAX) {
        return 1;
    }

    handlers[num_handlers++] = (struct atexit_handler) {
        .arg = arg,
        .advanced = function,
    };

    // TODO: unlock

    return 0;
}

static void process_atexit(int status) {
    /*
     * Do it this way, as atexit handlers are allowed to register new handlers,
     * that must be put at the front of the queue.
     */
    while (num_handlers > 0) {
        --num_handlers;

        struct atexit_handler handler = handlers[num_handlers];
        if (handler.advanced == NULL) {
            handler.standard();
        } else {
            handler.advanced(status, handler.arg);
        }
    }
}

void exit(int status) {
    // TODO: is there an order in which these exit steps must be done??

    process_atexit(status);
    fflush(NULL);
    // TODO: close all stdio files 
    //      -> (krnl will cleanup all remaining fd anyway when it destroys the
    //          filedes, but the C standard says exit() must close streams first)
    // TODO: all files created with tmpfile must be removed
    _exit(status);

    while (true) {
        ;
    }
}

void abort(void) {
    //raise(SIGABRT);
    //restore SIGABRT to default
    //raise(SIGABRT)

    _exit(-1);
    while (true) {
        ;
    }
}