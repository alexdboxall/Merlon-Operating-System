
#include <machine/regs.h>

void x86HandleInterrupt(struct x86_regs* r) {
    (void) r;
    
    // TODO: send EOIs, call functions, etc.
}