#nasm ld/init.s -o ld/init.o -felf32
/Users/alex/Desktop/NOS/toolchain/output/bin/i386-elf-gcc -c src/init.c -I"include" -I"../../kernel/include" -nostdlib -ffreestanding
/Users/alex/Desktop/NOS/toolchain/output/bin/i386-elf-gcc -c src/util.c -I"include" -I"../../kernel/include" -nostdlib -ffreestanding
/Users/alex/Desktop/NOS/toolchain/output/bin/i386-elf-gcc -c src/alloc.c -I"include" -I"../../kernel/include" -nostdlib -ffreestanding
/Users/alex/Desktop/NOS/toolchain/output/bin/i386-elf-gcc -c src/printf.c -I"include" -I"../../kernel/include" -nostdlib -ffreestanding
/Users/alex/Desktop/NOS/toolchain/output/bin/i386-elf-gcc -T ld/x86.ld *.o -o ../output/bootload.exe -nostdlib -ffreestanding

rm *.o
