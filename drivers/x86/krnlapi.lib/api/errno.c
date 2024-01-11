
/*__thread*/ int _real_errno;

int* __thread_local_errno_() {
    return &_real_errno;
}