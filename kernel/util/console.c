
#include <console.h>
#include <dev.h>
#include <vfs.h>
#include <thread.h>
#include <virtual.h>
#include <string.h>
#include <log.h>
#include <message.h>
#include <video.h>
#include <stdlib.h>

static struct file* open_console_master;
static struct file* open_console_sub;

static bool console_initialised = false;

static uint8_t console_fg = 0x7;
static uint8_t console_bg = 0x0;

static int console_buffer_ptr = 0;
static char console_buffer[VID_MAX_PUTCHARS_LEN];

static void Flush(void) {
	if (console_buffer_ptr > 0) {
		struct video_msg msg = (struct video_msg) {
			.type = VIDMSG_PUTCHARS,
			.putchars = {.fg = console_fg, .bg = console_bg},
		};
		strncpy(msg.putchars.cs, console_buffer, VID_MAX_PUTCHARS_LEN);
		SendVideoMessage(msg);
		console_buffer_ptr = 0;
		memset(console_buffer, 0, VID_MAX_PUTCHARS_LEN);
	}
}

static void Putchar(char c) {
	console_buffer[console_buffer_ptr++] = c;	
	if (console_buffer_ptr >= VID_MAX_PUTCHARS_LEN) {
		Flush();
	}
}

static void ProcessAnsiEscapeCode(char* code) {
	if (code[0] != '[') {
		return;
	}

	if (!strcmp(code, "[2J")) {				
		/* Clear screen */
		console_buffer_ptr = 0;
		memset(console_buffer, 0, VID_MAX_PUTCHARS_LEN);

		SendVideoMessage((struct video_msg) {
			.type = VIDMSG_CLEAR_SCREEN,
			.clear = {.bg = console_bg, .fg = console_fg}
		});

	} else if (!strcmp(code, "[0m")) {		
		/* Reset attributes */
		Flush();
		console_bg = 0x0;
		console_fg = 0x7;

	} else if (strlen(code) >= 4 && code[strlen(code) - 1] == 'm') {
		/* Set fg/bg colours */
		Flush(); 
		uint8_t map[8] = {0, 4, 2, 6, 1, 5, 3, 7};
		int col = atoi(code + 1);
		if (col >= 30 && col <= 37) {
			console_fg = map[col - 30];
		} else if (col >= 40 && col <= 47) {
			console_bg = map[col - 40];
		} else if (col >= 90 && col <= 97) {
			console_fg = map[col - 90] + 8;
		} else if (col >= 100 && col <= 107) {
			console_bg = map[col - 100] + 8;
		}

	} else if (strlen(code) >= 4 && code[strlen(code) - 1] == 'H') { 	
		/* Move cursor*/
		int index = 1;
		int y = atoi(code + index);
		while (code[index] && code[index] != ';') {
			++index;
		}
		if (code[index] == ';' && code[index + 1] != 0) {
			int x = atoi(code + index + 1);
			Flush();
			SendVideoMessage((struct video_msg) {
				.type = VIDMSG_SET_CURSOR,
				.setcursor = {.x = x - 1, .y = y - 1},
			});
		}

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

	const int TRANSFER_SIZE = 256;

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

		Flush();
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

	console_buffer_ptr = 0;
	memset(console_buffer, 0, VID_MAX_PUTCHARS_LEN);
}

void SendKeystrokeConsole(char c) {
	if (console_initialised) {
		struct transfer tr = CreateKernelTransfer(&c, 1, 0, TRANSFER_WRITE);
		WriteFile(open_console_master, &tr);
	}
}
