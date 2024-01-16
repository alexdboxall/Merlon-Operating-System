#pragma once

#include <common.h>
#include <sys/types.h>
#include <file.h>
#include <vnode.h>
#include <transfer.h>

void InitVfs(void);
int AddVfsMount(struct vnode* node, const char* name);
int RemoveVfsMount(const char* name);

int OpenFile(const char* path, int flags, mode_t mode, struct file** out);
int ReadFile(struct file* file, struct transfer* io);
int WriteFile(struct file* file, struct transfer* io);
int CloseFile(struct file* file);
int RemoveFileOrDirectory(const char* path, bool rmdir);