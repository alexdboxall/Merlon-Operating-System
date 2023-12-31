
DRIVER_NAME = progload.exe

CC = /Users/alex/Desktop/NOS/toolchain/output/bin/i386-elf-gcc
AS = nasm
FAKE_CROSS_COMPILER = -m32 -I"." -I"../../../source/include" -I"../../../source"
COMPILE_FLAGS = -c -Os -fno-strict-aliasing -Wall -Wextra -Wpedantic -Werror -Wcast-align=strict -Wpointer-arith -fmax-errors=5 -std=gnu2x -ffreestanding $(FAKE_CROSS_COMPILER)
LINK_FLAGS = -Wl,-Map=progload.map -nostartfiles -nostdlib -lgcc 

COBJECTS = $(patsubst %.c, %.o, $(wildcard *.c) $(wildcard */*.c) $(wildcard */*/*.c) $(wildcard */*/*/*.c) $(wildcard **/*.c))
ASMOBJECTS = $(patsubst %.s, %.oo, $(wildcard *.s) $(wildcard */*.s) $(wildcard */*/*.s) $(wildcard */*/*/*.s) $(wildcard **/*.s))

build: $(COBJECTS) $(ASMOBJECTS)
	$(CC) -T ../../../source/machine/progload.ld -o $(DRIVER_NAME) $^ $(LINK_FLAGS)
	cp $(DRIVER_NAME) ../../../output/drivers
	
%.o: %.c
	$(CC) $(CPPDEFINES) $(COMPILE_FLAGS) $^ -o $@ 
	rm $^

%.oo: %.s
	$(AS) -felf32 $^ -o $@ 
	rm $^
