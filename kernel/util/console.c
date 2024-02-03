
#include <console.h>
#include <dev.h>
#include <vfs.h>
#include <thread.h>
#include <virtual.h>
#include <string.h>
#include <log.h>
#include <message.h>
#include <video.h>

static struct file* open_console_master;
static struct file* open_console_sub;

static bool console_initialised = false;

static uint8_t console_fg = 0x7;
static uint8_t console_bg = 0x0;

static void Putchar(char c) {
	SendVideoMessage((struct video_msg) {
		.type = VIDMSG_PUTCHAR,
		.putchar = {.fg = console_fg, .bg = console_bg, .c = c},
	});
}

static void ProcessAnsiEscapeCode(char* code) {
	if (!strcmp(code, "[2J")) {				/* Clear screen */
		SendVideoMessage((struct video_msg) {
			.type = VIDMSG_CLEAR_SCREEN,
			.clear = {.bg = console_bg, .fg = console_fg}
		});

	} else if (!strcmp(code, "[0m")) {		/* Reset attributes */
		console_bg = 0x0;
		console_fg = 0x7;

	} else if (!strcmp(code, "[90m")) {		/* Black foreground */
		console_fg = 0x0;

	} else if (!strcmp(code, "[107m")) {	/* White background */
		console_bg = 0xF;

	} else if (!strcmp(code, "[1;1H")) {	/* Move cursor */
		// TODO: move cursor to top left

	} else {
		/*
		 * Not recognised as an escape sequence, so just print it to the screen.
		 */
		Putchar('\x1B');
		for (int i = 0; code[i]; ++i) {
			Putchar(code[i]);
		}
	}
}

static void ConsoleDriverThread(void*) {
	bool escape = false;
	char ansi_code[16];
	int ansi_code_len = 0;

	const int TRANSFER_SIZE = 128;

    while (true) {
        char buffer[TRANSFER_SIZE];
        struct transfer tr = CreateKernelTransfer(buffer, TRANSFER_SIZE, 0, TRANSFER_READ);
		ReadFile(open_console_master, &tr);

		for (size_t i = 0; i < TRANSFER_SIZE - tr.length_remaining; ++i) {
			char c = buffer[i];
			if (escape) {
				ansi_code[ansi_code_len++] = c;
				if ((c >= 0x40 && c != '[') || ansi_code_len >= 14) {
					ansi_code[ansi_code_len] = 0;
					ProcessAnsiEscapeCode(ansi_code);
					escape = false;
				}

			} else {
				if (c == '\x1B') {
					escape = true;
					ansi_code_len = 0;
				} else {
					Putchar(c);
				}
			}
		}
    }
}

void InitConsole(void) {
	struct vnode* console_master;
	struct vnode* console_sub;
	CreatePseudoTerminal(&console_master, &console_sub);
	
	open_console_master = CreateFile(console_master, 0, 0, true, true);
	open_console_sub = CreateFile(console_sub, 0, 0, true, true);
	AddVfsMount(console_sub, "con");
    CreateThread(ConsoleDriverThread, NULL, GetVas(), "con");
	console_initialised = true;
}

void SendKeystrokeConsole(char c) {
	if (console_initialised) {
		struct transfer tr = CreateKernelTransfer(&c, 1, 0, TRANSFER_WRITE);
		WriteFile(open_console_master, &tr);
	}
}