#pragma once

#include <common.h>

struct vnode;

void CreatePseudoTerminal(struct vnode** master, struct vnode** subordinate);