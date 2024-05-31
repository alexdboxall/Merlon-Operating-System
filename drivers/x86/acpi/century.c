
#include "acpi.h"
#include <log.h>
#include <stdint.h>
#include <machine/cmos.h>

void InitCenturyRegister(void) {
    uint8_t century_reg = AcpiGbl_FADT.Century;
    if (century_reg != 0) {
        LogWriteSerial("Century register is at CMOS: 0x%X\n", century_reg);

        uint8_t current_century = ReadCmos(century_reg);
        if (current_century < 20 || current_century > 23) {
            LogWriteSerial("Century register is very much out of range! (%d)\n", current_century);
        } else {
            x86_rtc_century_register = century_reg;
        }
    }
}