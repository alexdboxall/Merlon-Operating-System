#pragma once

#include <common.h>

#define PROC_MAX_FD 1024

struct file;
struct fd_table;

int CreateFd(struct fd_table* table, struct file* file, int* fd_out, int flags);
int RemoveFd(struct fd_table* table, struct file* file);
int GetFileFromFd(struct fd_table* table, int fd, struct file** out);

int HandleExecFd(struct fd_table* table);

struct fd_table* CreateFdTable(void);
struct fd_table* CopyFdTable(struct fd_table* original);
void DestroyFdTable(struct fd_table* table);

int DupFd(struct fd_table* table, int oldfd, int* newfd);
int DupFd2(struct fd_table* table, int oldfd, int newfd, int flags);
