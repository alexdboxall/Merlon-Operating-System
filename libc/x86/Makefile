
CC = i686-elf-gcc
AR = i686-elf-ar
AS = nasm
FAKE_CROSS_COMPILER = -m32 -I../common/include -I../hosted/include -I../common/include/openlibm -I"../../common"
COMPILE_FLAGS = -c -Os -ffunction-sections -fdata-sections -fno-strict-aliasing -Wall -Wextra -Wpedantic -Werror -Wcast-align=strict -Wpointer-arith -fmax-errors=5 -std=gnu11 -ffreestanding $(FAKE_CROSS_COMPILER)
LINK_FLAGS = -nostartfiles -nostdlib -lgcc 

# Set by the higher level Makefile before calling us - changes depending on whether we are compiling the debug or release build
LINKER_STRIP =
CPPDEFINES = 

COBJECTS = $(patsubst %.c, %.o, $(wildcard ../*.c) $(wildcard ../*/*.c) $(wildcard ../*/*/*.c) $(wildcard ../*/*/*/*.c) $(wildcard ../**/*.c))
ASMOBJECTS = $(patsubst %.s, %.oo, $(wildcard *.s) $(wildcard */*.s) $(wildcard */*/*.s) $(wildcard */*/*/*.s) $(wildcard **/*.s))

hostedlibc: $(COBJECTS) $(ASMOBJECTS)
	$(AR) rcs libc.a $^
	find ../common -type f -name '*.o' -delete || true
	find ../common -type f -name '*.oo' -delete || true
	find ../hosted -type f -name '*.o' -delete || true
	find ../hosted -type f -name '*.oo' -delete || true
	
%.o: %.c
	$(CC) $(CPPDEFINES) $(COMPILE_FLAGS) $^ -o $@ 
	
%.oo: %.s
	$(AS) -felf32 $^ -o $@ 
