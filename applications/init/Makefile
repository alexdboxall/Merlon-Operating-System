
APPLICATION_NAME = init.exe

CC = /Users/alex/Desktop/NOS/toolchain/output/bin/i386-elf-gcc
FAKE_CROSS_COMPILER = -m32 -I"." -I"../../source/include" -I"../../source"
COMPILE_FLAGS = -c -Os -fno-strict-aliasing -Wall -Wextra -Wpedantic -Werror -Wcast-align=strict -Wpointer-arith -fmax-errors=5 -std=gnu2x -ffreestanding $(FAKE_CROSS_COMPILER)
LINK_FLAGS = -Wl,-Map=app.map -nostartfiles -nostdlib -lc -lgcc

COBJECTS = $(patsubst %.c, %.o, $(wildcard *.c) $(wildcard */*.c) $(wildcard */*/*.c) $(wildcard */*/*/*.c) $(wildcard **/*.c))

build: $(COBJECTS)
	$(CC) -T ../../source/machine/application.ld -o $(APPLICATION_NAME) -L"../../../libc/x86" -L"../../output/drivers" -l:krnlapi.lib $^ $(LINK_FLAGS) $(LINKER_STRIP)
	objdump -drwC -Mintel $(APPLICATION_NAME) >> disassembly.txt
	cp $(APPLICATION_NAME) ../../output/applications
	
%.o: %.c
	$(CC) $(CPPDEFINES) $(COMPILE_FLAGS) $^ -o $@ 