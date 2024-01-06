
make osrelease
qemu-system-i386 -audiodev coreaudio,id=audio0 -machine pcspk-audiodev=audio0 -monitor stdio -fda build/output/floppy.img -serial file:log.txt -m 14096k -rtc base=localtime -d guest_errors,cpu_reset 

read -p "Press Enter to continue" </dev/tty
