
make osrelease
qemu-system-i386 -m 4M -audiodev coreaudio,id=audio0 -machine pcspk-audiodev=audio0 -monitor stdio -hda build/output/disk.bin -serial file:log.txt -rtc base=localtime -d guest_errors,cpu_reset 

rem -fda build/output/floppy.img

read -p "Press Enter to continue" </dev/tty
