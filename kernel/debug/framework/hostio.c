
#ifndef NDEBUG

#include <common.h>
#include <debug/hostio.h>
#include <log.h>
#include <assert.h>

static void outb(uint16_t port, uint8_t value)
{
	asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port)
{
	uint8_t value;
	asm volatile ("inb %1, %0"
		: "=a"(value)
		: "Nd"(port));
	return value;
}

static void DbgWriteByte(uint8_t value) {
    while ((inb(0x3E8 + 5) & 0x20) == 0) {
		;
	}
	outb(0x3E8, value);
}

static uint8_t DbgReadByte() {
    while ((inb(0x2F8 + 5) & 1) == 0) {
        ;
    }
    return inb(0x2F8);
}

void DbgWritePacket(int type, uint8_t* data, int size) {
    DbgWriteByte(0xAA);
    DbgWriteByte(type);
    DbgWriteByte((size >> 16) & 0xFF);
    DbgWriteByte((size >> 8) & 0xFF);
    DbgWriteByte((size >> 0) & 0xFF);
    DbgWriteByte(0xBB);

    for (int i = 0; i < size; ++i) {
        DbgWriteByte(data[i]);
    }

    DbgWriteByte(0xCC);
}

void DbgReadPacket(int* type, uint8_t* data, int* size) {
    uint8_t v = DbgReadByte();
    if (v != 0xAA) {
        LogDeveloperWarning("Huh? Got 0x%X instead of 0xAA...\n", v);
    }
    assert(v == 0xAA);

    *type = DbgReadByte();

    int size_tmp = DbgReadByte();
    size_tmp <<= 8;
    size_tmp |= DbgReadByte();
    size_tmp <<= 8;
    size_tmp |= DbgReadByte();

    int max_size = *size;

    *size = size_tmp;

    v = DbgReadByte();
    assert(v == 0xBB);

    for (int i = 0; i < size_tmp; ++i) {
        if (i < max_size) {
            data[i] = DbgReadByte();
        }
    }
    
    v = DbgReadByte();
    assert(v == 0xCC);
}

#else
extern int make_iso_compilers_happy;
#endif