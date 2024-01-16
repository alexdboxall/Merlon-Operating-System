#pragma once

void InitNullDevice(void);
void InitRandomDevice(void);

struct vnode;

void CreatePseudoTerminal(struct vnode** master, struct vnode** subordinate);

struct vnode* CreatePipe(void);
void BreakPipe(struct vnode* node);