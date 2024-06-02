## Merlon - Operating System ##

Merlon is an operating system (OS) based on my earlier [ATOS](https://github.com/alexdboxall/ATOS), which was in turn inspired by OS/161. Merlon is written in C, and is designed (relatively) easy to understand, portable and lightweight. Unlike ATOS which was aiming to be more of an "educational" OS, Merlon tries to be a more full featured OS (e.g. the virtual memory manager is has a lot more features), and I decided I liked the `WindowsNamingConvention()` instead of the `unix_naming_convention`.

Merlon still only requires around 3MB of RAM to run, and excluding ACPICA and FAT drivers, is only 50,000 lines of commented code.

It is currently only implemented for x86, but should be easy to port to other platforms (via the arch/ folder, and arch.h).

To build it, run `./release.sh`. To run it in QEMU, use the following command: `qemu-system-i386 -soundhw pcspk -hda build/output/disk.bin -m 3M`

Some features include:
- Fully multithreaded kernel
- Custom standard C library
- Relocatable and dynamically linked drivers
- A dynamically linked library for accessing the kernel
- Support for signals
- ACPICA driver
- Page swapping when low on memory
- Message passing
- A virtual filesystem (VFS) to manage files, folders and devices
- Custom second-stage bootloader written in C for portability (uses an abstraction library loaded in stage one)
- Supports i486 and later processors, only needs 3MiB RAM to run
- Drivers so far: PS/2, ATA, RTC, ACPI
- Filesystem support: DemoFS, (FAT support coming)

The TODO list (vaguely in order)
- `fork`
- Writing a "decent" shell
- FAT support using FatFS
- "Homemade" FAT driver (instead of using FatFS) - for code style consistency and better integration
- `<pthread.h>`
- Disk auto-detection
- A GUI!
- Fixing low memory crashes
- Disk caching
- Better kernel support and testing for `EINTR`
- `sigaction`, `sigprocmask`, etc.
- Other system calls and functions, ...
- Fixing up all the other TODOs in the code!!

![Merlon Kernel](https://github.com/alexdboxall/Merlon/blob/main/docs/assets/readme/b.jpg "Merlon Kernel")

Copyright Alex Boxall 2022-2024. See LICENSE for details.

Merlon is named after the character from *Super Paper Mario*, Merlon.

![Merlon](https://github.com/alexdboxall/Merlon/blob/main/docs/assets/logo/merlon.png)
