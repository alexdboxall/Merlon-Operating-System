#pragma once

#include <common.h>

struct msgbox;

struct msgbox* CreateMessageBox(const char* name, int payload_size);
void DestroyMessageBox(struct msgbox* mbox);
int SendMessage(struct msgbox* mbox, void* payload);
int ReceiveMessage(struct msgbox* mbox, void* payload);