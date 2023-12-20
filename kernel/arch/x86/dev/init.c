#include <machine/dev.h>
#include <machine/ps2controller.h>

void ArchInitDev(bool fs) {
    if (!fs) {
        InitIde();
        InitPs2();
    } else {

    }

}