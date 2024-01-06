
#include <common.h>
#include <console.h>
#include <log.h>
#include <string.h>
#include <irq.h>
#include <irql.h>
#include <errno.h>
#include <machine/pic.h>
#include <machine/portio.h>
#include <machine/interrupt.h>
#include "ps2keyboard.h"

#define PS2_STATUS_BIT_OUT_FULL		1
#define PS2_STATUS_BIT_IN_FULL		2
#define PS2_STATUS_BIT_SYSFLAG		4
#define PS2_STATUS_BIT_CONTROLLER	8
#define PS2_STATUS_BIT_TIMEOUT		64
#define PS2_STATUS_BIT_PARITY		128

static int LOCKED_DRIVER_CODE Ps2Wait(bool writing) {
    int timeout = 0;
    while (true) {
        uint8_t status = inb(0x64);

        if (writing) {
            if (!(status & PS2_STATUS_BIT_IN_FULL)) {
                break;
            }
        } else {
            if (status & PS2_STATUS_BIT_OUT_FULL) {
                break;
            }
        }

        if (++timeout >= 2000 || (status & PS2_STATUS_BIT_TIMEOUT) || (status & PS2_STATUS_BIT_PARITY)) {
            return EIO;
        }
    } 

    return 0;
}

/*
* For all of these, we are going to ignore error checking and proceed
* regardless, as the error detection can be a bit unreliable on real
* hardware and between different emulators. (This is thanks to the
* various quality of USB to PS/2 emulation implementations.)
*/
static void LOCKED_DRIVER_CODE Ps2ControllerWrite(uint8_t data) {
    outb(0x64, data);
}

static void LOCKED_DRIVER_CODE Ps2ControllerWrite2(uint8_t data1, uint8_t data2) {
    Ps2ControllerWrite(data1);
    Ps2Wait(true);
    outb(0x60, data2);
}

static uint8_t LOCKED_DRIVER_CODE Ps2ControllerRead(void) {
    Ps2Wait(false);
    return inb(0x60);
}

uint8_t LOCKED_DRIVER_CODE Ps2ControllerGetConfiguration(void) {
    Ps2ControllerWrite(0x20);
    return Ps2ControllerRead();
}

void LOCKED_DRIVER_CODE Ps2ControllerSetConfiguration(uint8_t value) {
    Ps2ControllerWrite2(0x60, value);
}

uint8_t LOCKED_DRIVER_CODE Ps2DeviceRead(void) {
    return Ps2ControllerRead();
}

int LOCKED_DRIVER_CODE Ps2DeviceWrite(uint8_t data, bool port2) {
    if (port2) {
        Ps2ControllerWrite(0xD4);
    }
    Ps2Wait(true);

    int timeout = 10;
    while (timeout) {
        outb(0x60, data);

        uint8_t ack = Ps2DeviceRead();
        if (ack == 0xFA) {
            return 0;
        } else if (ack == 0xFE) {
            --timeout;
            LogWriteSerial("PS/2 asked for retry...\n");
        } else {
            LogDeveloperWarning("Weird PS/2 response: 0x%X\n", ack);
            return EIO;
        }
    }

    LogDeveloperWarning("PS/2 asked for retry 10 times... aborting command\n");
    return ETIMEDOUT;
}

void Ps2ControllerDisableDevice(bool port2) {
    Ps2ControllerWrite(port2 ? 0xA7 :0xAD);
}

void Ps2ControllerEnableDevice(bool port2) {
    Ps2ControllerWrite(port2 ? 0xA8 : 0xAE);
}

static int Ps2ControllerPerformSelfTest(void) {
    Ps2ControllerWrite(0xAA);
    return (Ps2ControllerRead() == 0x55) ? 0 : EIO;
}

int Ps2ControllerTestPort(bool port2) {
    Ps2ControllerWrite(port2 ? 0xA9 : 0xAB);
    return (Ps2ControllerRead() == 0x00) ? 0 : EIO;
}

static bool Ps2ControllerDetectPort2(void) {
    Ps2ControllerDisableDevice(true);
    uint8_t config = Ps2ControllerGetConfiguration();
    if (!(config & (1 << 5))) {
        return false;
    }

    Ps2ControllerEnableDevice(true);
    config = Ps2ControllerGetConfiguration();
    if (config & (1 << 5)) {
        return false;
    }

    Ps2ControllerDisableDevice(true);
    return true;
}

void LOCKED_DRIVER_CODE Ps2ControllerSetIrqEnable(bool enable, bool port2) {
    uint8_t config = Ps2ControllerGetConfiguration();
    if (enable) {
        config |= (port2 ? 2 : 1);
    } else {
        config &= ~(port2 ? 2 : 1);
    }
    Ps2ControllerSetConfiguration(config);
}

void InitPs2(void) {
    Ps2ControllerDisableDevice(false);
    Ps2ControllerDisableDevice(true);
    Ps2ControllerRead();

    int res = Ps2ControllerPerformSelfTest();
    if (res != 0) {
        LogDeveloperWarning("PS/2 self test failed!\n");
        return;
    }

    /*
     * Self-test can sometimes reset the controller, so re-disable devices.
     */
    Ps2ControllerDisableDevice(false);
    Ps2ControllerDisableDevice(true);
    Ps2ControllerRead();

    Ps2ControllerSetIrqEnable(false, false);
    Ps2ControllerSetIrqEnable(false, true);

    bool has_port2 = Ps2ControllerDetectPort2();

    InitPs2Keyboard();
    if (has_port2) {
        LogWriteSerial("we have a second PS/2 port\n");
        // InitPs2Mouse();
    }
}