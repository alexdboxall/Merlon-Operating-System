
#include "acpi.h"
#include <log.h>
#include <stdint.h>
#include <machine/cmos.h>

static bool HasCorrectParity(uint8_t val) {
    /*
     * Odd parity is correct.
     */
    bool odd = false;
    for (int i = 0; i < 8; ++i) {
        if (val & 1) {
            odd ^= 1;
        }
        val >>= 1;
    }
    return odd;
}

void InitSimpleBootFlag(void) {
    ACPI_TABLE_HEADER* sbf;
    ACPI_STATUS res = AcpiGetTable("BOOT", 1, &sbf);
    if (res == AE_OK) {
        LogWriteSerial("`BOOT` table for simple boot flag is at 0x%X\n", sbf);

        uint8_t cmos_addr = ((uint8_t*) sbf)[36];
        uint8_t byte = ReadCmos(cmos_addr);

        if (byte & 0b01110000) {
            LogWriteSerial("Simple boot flag has reserved bits set...\n");
        } else if (!HasCorrectParity(byte)) {
            LogWriteSerial("Simple boot flag has invalid parity...\n");
        } else {
            /*
             * Set boot as successful, disable diagostics, give OS control of
             * the display, and mark as PnP compatibile.
             */
            byte &= ~(2 | 4 | 8);      // 2=BOOTING, 4=DIAG, 8=SCREEN
            byte |= 1;                 // 1=PNP
            if (!HasCorrectParity(byte)) {
                byte ^= 0x80;
            }

            LogWriteSerial("Setting simple boot flag to 0x%X...\n", byte);
            WriteCmos(cmos_addr, byte);
        }
    }
}