
static int _real_errno;

/*
 * TODO: make this per-thread.
 */
int* __thread_local_errno_() {
    return &_real_errno;
}