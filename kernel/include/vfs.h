#pragma once

#include <common.h>
#include <sys/types.h>
#include <openfile.h>
#include <vnode.h>
#include <transfer.h>

void InitVfs(void);
int AddVfsMount(struct vnode* node, const char* name);
int RemoveVfsMount(const char* name);

int OpenFile(const char* path, int flags, mode_t mode, struct open_file** out);
int ReadFile(struct open_file* file, struct transfer* io);
int ReadDirectory(struct open_file* file, struct transfer* io);
int WriteFile(struct open_file* file, struct transfer* io);
int CloseFile(struct open_file* file);
