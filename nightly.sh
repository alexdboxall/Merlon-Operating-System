
make osdebug
rm build/dbgpipe_osread || true
rm build/dbgpipe_oswrite.txt || true
mkfifo build/dbgpipe_osread
mkfifo build/dbgpipe_oswrite
python tools/debug.py --nightly &
qemu-system-i386  -monitor stdio -nographic -hda build/output/disk.bin -serial file:log.txt -serial pipe:build/dbgpipe_osread -serial file:build/dbgpipe_oswrite.txt -m 4 -rtc base=localtime -d guest_errors,cpu_reset
read -p "Press Enter to continue" </dev/tty
