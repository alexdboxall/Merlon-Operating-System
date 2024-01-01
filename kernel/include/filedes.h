#pragma once

#include <common.h>

#define MAX_FD_PER_PROCESS 200

struct open_file;
struct filedes_table;

int CreateFileDescriptor(struct filedes_table* table, struct open_file* file, int* fd_out, int flags);
int RemoveFileDescriptor(struct filedes_table* table, struct open_file* file);
int GetFileFromDescriptor(struct filedes_table* table, int fd, struct open_file** out);

int HandleFileDescriptorsOnExec(struct filedes_table* table);

struct filedes_table* CreateFileDescriptorTable(void);
struct filedes_table* CopyFileDescriptorTable(struct filedes_table* original);
int DuplicateFileDescriptor(struct filedes_table* table, int oldfd, int* newfd);
int DuplicateFileDescriptor2(struct filedes_table* table, int oldfd, int newfd);
int DuplicateFileDescriptor3(struct filedes_table* table, int oldfd, int newfd, int flags);