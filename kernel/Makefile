
CC = /Users/alex/Desktop/NOS/toolchain/output/bin/i386-elf-gcc
AS = nasm
FAKE_CROSS_COMPILER = -m32 -I"." -Iinclude -Imachine/include -Iinclude/openlibm
COMPILE_FLAGS = -c -Os -fipa-icf -std=gnu2x -ffunction-sections -fno-strict-aliasing -DCOMPILE_KERNEL -Wall -Wextra -Wpedantic -Werror -Wcast-align=strict -Wpointer-arith -fmax-errors=5 -ffreestanding $(FAKE_CROSS_COMPILER) -Wno-infinite-recursion -fomit-frame-pointer
#-flto -finstrument-functions
LINK_FLAGS = -fuse-ld=gold -Wl,--icf=all -Wl,-Map=kernel.map -nostartfiles -nostdlib -lgcc  

# Set by the higher level Makefile before calling us - changes depending on whether we are compiling the debug or release build
LINKER_STRIP =
CPPDEFINES = 

COBJECTS = $(patsubst %.c, %.o, $(wildcard *.c) $(wildcard */*.c) $(wildcard */*/*.c) $(wildcard */*/*/*.c) $(wildcard */*/*/*/*.c) $(wildcard **/*.c))
ASMOBJECTS = $(patsubst %.s, %.oo, $(wildcard *.s) $(wildcard */*.s) $(wildcard */*/*.s) $(wildcard */*/*/*.s) $(wildcard **/*.s))

oskernel: $(COBJECTS) $(ASMOBJECTS) $(HOBJECTS)
	$(CC) -T machine/linker.ld -o KERNEL.EXE $^ $(LINK_FLAGS) $(LINKER_STRIP)
	rm -r include
	rm -r machine
	rm Makefile

%.o: %.c
	$(CC) $(CPPDEFINES) $(COMPILE_FLAGS) $^ -o $@ 
	rm $^

%.oo: %.s
	$(AS) -felf32 $^ -o $@ 
	rm $^
