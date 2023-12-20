#include <machine/dev.h>
#include <machine/ps2controller.h>

void ArchInitDev(void) {
    InitIde();
    InitPs2();
}