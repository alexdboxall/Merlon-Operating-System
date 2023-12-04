
#include <console.h>
#include <pty.h>
#include <vfs.h>
#include <thread.h>
#include <virtual.h>
#include <string.h>
#include <log.h>

static struct vnode* console_master;
static struct vnode* console_subordinate;

static struct open_file* open_console_master;
static struct open_file* open_console_subordinate;

static bool console_initialised = false;

/*
 * NOTE: the console only echos input when there's someone waiting for input
 *		 (which should be fine most of the time - the only people input is when it's waiting for input!)
 */

static void ConsoleDriverThread(void*) {
	AddVfsMount(console_subordinate, "con");

    while (true) {
        char c;
        struct transfer tr = CreateKernelTransfer(&c, 1, 0, TRANSFER_READ);
		ReadFile(open_console_master, &tr);
		DbgScreenPutchar(c);
    }
}

void InitConsole(void) {
	CreatePseudoTerminal(&console_master, &console_subordinate);
	open_console_master = CreateOpenFile(console_master, 0, 0, true, true);
	open_console_subordinate = CreateOpenFile(console_subordinate, 0, 0, true, true);
    CreateThread(ConsoleDriverThread, NULL, GetVas(), "con");
	console_initialised = true;
}

void SendKeystrokeConsole(char c) {
	if (!console_initialised) return;

	struct transfer tr = CreateKernelTransfer(&c, 1, 0, TRANSFER_WRITE);
	WriteFile(open_console_master, &tr);
}

char GetcharConsole(void) {
	if (!console_initialised) return 0;
	
	char c;
	struct transfer tr = CreateKernelTransfer(&c, 1, 0, TRANSFER_READ);
	ReadFile(open_console_subordinate, &tr);
	return c;
}

void PutcharConsole(char c) {
	if (!console_initialised) return;
	struct transfer tr = CreateKernelTransfer(&c, 1, 0, TRANSFER_WRITE);
	WriteFile(open_console_subordinate, &tr);
}

void PutsConsole(const char* s) {
	for (int i = 0; s[i]; ++i) {
		PutcharConsole(s[i]);
	}
}
