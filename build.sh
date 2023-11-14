
make osdebug
rm dbgpipe_osread || true
mkfifo dbgpipe_osread
python tools/debug.py &
qemu-system-i386 -audiodev coreaudio,id=audio0 -machine pcspk-audiodev=audio0  -monitor stdio -hda build/output/disk.bin -serial file:log.txt -serial pipe:dbgpipe_osread -serial pipe:dbgpipe_oswrite -m 4 -rtc base=localtime -d guest_errors,cpu_reset
read -p "Press Enter to continue" </dev/tty
