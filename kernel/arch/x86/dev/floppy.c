#include <common.h>
#include <semaphore.h>
#include <log.h>
#include <thread.h>
#include <vfs.h>
#include <string.h>
#include <transfer.h>
#include <assert.h>
#include <irq.h>
#include <errno.h>
#include <machine/pic.h>
#include <machine/portio.h>
#include <heap.h>
#include <virtual.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <irql.h>
#include <thread.h>
#include <errno.h>
#include <diskutil.h>

#define CYLINDER_SIZE (512 * 18 * 2)

#define FLOPPY_DOR      2
#define FLOPPY_MSR      4
#define FLOPPY_FIFO     5
#define FLOPPY_CCR      7

#define CMD_SPECIFY     0x03
#define CMD_READ        0x06
#define CMD_RECALIBRATE 0x07
#define CMD_SENSE_INT   0x08
#define CMD_SEEK        0x0F
#define CMD_CONFIGURE   0x13

struct semaphore* floppy_lock = NULL;

struct floppy_data {
    int disk_num;
    uint8_t* cylinder_buffer;
    uint8_t* cylinder_zero;
    size_t base;
    struct disk_partition_helper partitions;
    int stored_cylinder;
    bool got_cylinder_zero;
};

static void FloppyWriteCommand(struct floppy_data* flp, int cmd) {
    int base = flp->base;

    for (int i = 0; i < 60; ++i) {
        SleepMilli(10);
        if (inb(base + FLOPPY_MSR) & 0x80) {
            outb(base + FLOPPY_FIFO, cmd);
            return;
        }
    }
}

static uint8_t FloppyReadData(struct floppy_data* flp) {
    int base = flp->base;
    for (int i = 0; i < 60; ++i) {
        SleepMilli(10);
        if (inb(base + FLOPPY_MSR) & 0x80) {
            return inb(base + FLOPPY_FIFO);
        }
    }

    return 0;
}

static void FloppyCheckInterrupt(struct floppy_data* flp, int* st0, int* cyl) {
    FloppyWriteCommand(flp, CMD_SENSE_INT);
    *st0 = FloppyReadData(flp);
    *cyl = FloppyReadData(flp);
}

/*
 * The state can be 0 (off), 1 (on) or 2 (currently on, but will shortly be turned off).
 */
static volatile int floppy_motor_state = 0;
static volatile int floppy_motor_ticks = 0;

static void FloppyMotor(struct floppy_data* flp, bool state) {
    if (state) {
        if (!floppy_motor_state) {
            outb(flp->base + FLOPPY_DOR, 0x1C);
            SleepMilli(150);
        }
        floppy_motor_state = 1;

    } else {
        floppy_motor_state = 2;
        floppy_motor_ticks = 1000;
    }
}

static volatile bool floppy_got_irq = false;

static int FloppyIrqWait(bool allow_intr) {
    int timeout = 0;
    while (!floppy_got_irq) {
        SleepMilli(10);
        if (++timeout > 200) {
            return ETIMEDOUT;
        }
        if (allow_intr && HasBeenSignalled()) {
            return EINTR;
        }
    }

    floppy_got_irq = false;
    return 0;
}

static int FloppyIrqHandler(struct x86_regs*) {
    floppy_got_irq = true;
    return 0;
}

static void FloppyMotorControlThread(void*) {
    while (1) {
        SleepMilli(50);
        if (floppy_motor_state == 2) {
            floppy_motor_ticks -= 50;
            if (floppy_motor_ticks <= 0) {
                /*
                * Actually turn off the motor.
                */
                outb(0x3F0 + FLOPPY_DOR, 0x0C);
                floppy_motor_state = 0;
            }
        }
    }
}

/*
* Move to cylinder 0.
*/
static int FloppyCalibrate(struct floppy_data* flp) {
    LogWriteSerial("FloppyCalibrate\n");

    int st0 = -1;
    int cyl = -1;

    FloppyMotor(flp, true);

    for (int i = 0; i < 10; ++i) {
        FloppyWriteCommand(flp, CMD_RECALIBRATE);
        FloppyWriteCommand(flp, 0);

        FloppyIrqWait(false);
        FloppyCheckInterrupt(flp, &st0, &cyl);

        if (st0 & 0xC0) {
            continue;
        }

        if (cyl == 0) {
            FloppyMotor(flp, false);
            return 0;
        }
    }

    FloppyMotor(flp, false);
    return EIO;
}

static void FloppyConfigure(struct floppy_data* flp) {
    FloppyWriteCommand(flp, CMD_CONFIGURE);
    FloppyWriteCommand(flp, 0x00);
    FloppyWriteCommand(flp, 0x08);
    FloppyWriteCommand(flp, 0x00);
}

static int FloppyReset(struct floppy_data* flp) {
    int base = flp->base;
    outb(base + FLOPPY_DOR, 0x00);
    outb(base + FLOPPY_DOR, 0x0C);

    FloppyIrqWait(false);

    for (int i = 0; i < 4; ++i) {
        int st0, cyl;
        FloppyCheckInterrupt(flp, &st0, &cyl);
    }

    outb(base + FLOPPY_CCR, 0x00);

    FloppyMotor(flp, true);

    FloppyWriteCommand(flp, CMD_SPECIFY);
    FloppyWriteCommand(flp, 0xDF);
    FloppyWriteCommand(flp, 0x02);

    SleepMilli(300);
    FloppyConfigure(flp);
    SleepMilli(300);
    FloppyMotor(flp, false);

    return FloppyCalibrate(flp);
}

static int FloppySeek(struct floppy_data* flp, int cylinder, int head) {
    int st0, cyl;
    FloppyMotor(flp, true);

    for (int i = 0; i < 10; ++i) {
        FloppyWriteCommand(flp, CMD_SEEK);
        FloppyWriteCommand(flp, head << 2);
        FloppyWriteCommand(flp, cylinder);

        FloppyIrqWait(false);
        FloppyCheckInterrupt(flp, &st0, &cyl);

        if (st0 & 0xC0) {
            continue;
        }

        if (cyl == cylinder) {
            FloppyMotor(flp, false);
            return 0;
        }
    }

    FloppyMotor(flp, false);
    return EIO;
}

static void FloppyDmaInit(void) {
    /*
    * Put the data at *physical address* 0x10000. The address can be anywhere 
    * under 24MB that doesn't cross a 64KB boundary. We choose this location as 
    * it should be unused as this is where the temporary copy of the kernel was
    * stored during boot.
    */
    uint32_t addr = (uint32_t) 0x10000;

    /*
    * We must give the DMA the actual count minus 1.
    */
    int count = 0x4800 - 1;

    /*
    * Send some magical stuff to the DMA controller.
    */
    outb(0x0A, 0x06);
    outb(0x0C, 0xFF);
    outb(0x04, (addr >> 0) & 0xFF);
    outb(0x04, (addr >> 8) & 0xFF);
    outb(0x81, (addr >> 16) & 0xFF);
    outb(0x0C, 0xFF);
    outb(0x05, (count >> 0) & 0xFF);
    outb(0x05, (count >> 8) & 0xFF);
    outb(0x0B, 0x46);
    outb(0x0A, 0x02);
}

static int FloppyDoCylinder(struct floppy_data* flp, int cylinder) {
    /*
    * Move both heads to the correct cylinder.
    */
    if (FloppySeek(flp, cylinder, 0) != 0) return EIO;
    if (FloppySeek(flp, cylinder, 1) != 0) return EIO;

    /*
    * This time, we'll try up to 20 times.
    */
    for (int i = 0; i < 20; ++i) {
        FloppyMotor(flp, true);

        if (i % 5 == 3) {
            if (i % 10 == 8) {
                FloppyReset(flp);
                FloppyMotor(flp, true);
            }

            FloppyCalibrate(flp);

            if (FloppySeek(flp, cylinder, 0) != 0) return EIO;
            if (FloppySeek(flp, cylinder, 1) != 0) return EIO;
        }

        FloppyDmaInit();

        SleepMilli(100);

        /*
        * Send the read command.
        */
        FloppyWriteCommand(flp, CMD_READ | 0xC0);
        FloppyWriteCommand(flp, 0);
        FloppyWriteCommand(flp, cylinder);
        FloppyWriteCommand(flp, 0);
        FloppyWriteCommand(flp, 1);
        FloppyWriteCommand(flp, 2);
        FloppyWriteCommand(flp, 18);
        FloppyWriteCommand(flp, 0x1B);
        FloppyWriteCommand(flp, 0xFF);

        FloppyIrqWait(false);

        /*
        * Read back some status information, some of which is very mysterious.
        */
        uint8_t st0, st1, st2, rcy, rhe, rse, bps;
        st0 = FloppyReadData(flp);
        st1 = FloppyReadData(flp);
        st2 = FloppyReadData(flp);
        rcy = FloppyReadData(flp);
        rhe = FloppyReadData(flp);
        rse = FloppyReadData(flp);
        bps = FloppyReadData(flp);

        /*
        * Check for errors. More tests can be done, but it would make the code
        * even longer.
        */
        if (st0 & 0xC0) {
            continue;
        }
        if (st1 & 0x80) {
            continue;
        }
        if (st0 & 0x8) {
            continue;
        }
        if (st1 & 0x20) {
            continue;
        }
        if (st1 & 0x10) {
            continue;
        }
        if (bps != 2) {
            continue;
        }

        (void) st2;
        (void) rcy;
        (void) rhe;
        (void) rse;

        FloppyMotor(flp, false);
        return 0;
    }

    FloppyMotor(flp, false);
    return EIO;
}

static int FloppyIo(struct floppy_data* flp, struct transfer* io) {
    EXACT_IRQL(IRQL_STANDARD);

    if (io->direction == TRANSFER_WRITE) {
        return EROFS;
    }

    int lba = io->offset / 512;
    int count = io->length_remaining / 512;

    if (io->offset % 512 != 0) {
        return EINVAL;
    }
    if (io->length_remaining % 512 != 0) {
        return EINVAL;
    }
    if (count <= 0 || count > 0xFF || lba < 0 || lba >= 2880) {
        return EINVAL;
    }

    AcquireMutex(floppy_lock, -1);

next_sector:;
    /*
    * Floppies use CHS (cylinder, head, sector) for addressing sectors instead 
    * of LBA (linear block addressing). Hence we need to convert to CHS.
    * Note that sector is 1-based, whereas cylinder and head are 0-based.
    */
    int head = (lba % (18 * 2)) / 18;
    int cylinder = (lba / (18 * 2));
    int sector = (lba % 18) + 1;

    /*
    * Cylinder 0 has some commonly used data, so cache it seperately for 
    * improved speed.
    */
    if (cylinder == 0) {
        if (!flp->got_cylinder_zero) {
            flp->got_cylinder_zero = true;

            int error = FloppyDoCylinder(flp, cylinder);

            if (error != 0) {
                ReleaseMutex(floppy_lock);
                return error;
            }

            memcpy(flp->cylinder_zero, flp->cylinder_buffer, 0x4800);

            /* 
            * Must do this as we trashed the cached cylinder.
            * Luckily we only ever need to do this once.
            */
            flp->stored_cylinder = cylinder;
        }
    
        PerformTransfer(flp->cylinder_zero + (512 * (sector - 1 + head * 18)), io, 512);

    } else {
        if (cylinder != flp->stored_cylinder) {
            int error = FloppyDoCylinder(flp, cylinder);

            if (error != 0) {
                ReleaseMutex(floppy_lock);
                return error;
            }
        }

        PerformTransfer(flp->cylinder_buffer + (512 * (sector - 1 + head * 18)), io, 512);
        flp->stored_cylinder = cylinder;
    }
    
    /*
    * Read only read one sector, so we need to repeat the process if multiple 
    * sectors were requested. We could just do a larger copy above, but this is 
    * a bit simpler, as we don't need to worry about whether the entire request 
    * is on cylinder or not.
    */
    if (count > 0) {
        ++lba;
        --count;
        goto next_sector;
    }

    ReleaseMutex(floppy_lock);
    return 0;
}

static int ReadWrite(struct vnode* node, struct transfer* io) {
    return FloppyIo(node->data, io);
}

static int Create(struct vnode* node, struct vnode** partition, const char* name, int, mode_t) {
    AcquireMutex(floppy_lock, -1);
    struct floppy_data* flp = node->data;
    int res = DiskCreateHelper(&flp->partitions, partition, name);
    ReleaseMutex(floppy_lock);
    return res;
}

static int Follow(struct vnode* node, struct vnode** output, const char* name) {
    AcquireMutex(floppy_lock, -1);
    struct floppy_data* flp = node->data;
    int res = DiskFollowHelper(&flp->partitions, output, name);
    ReleaseMutex(floppy_lock);
    return res;
}

static const struct vnode_operations dev_ops = {
    .read           = ReadWrite,
    .write          = ReadWrite,
    .create         = Create,
    .follow         = Follow,
};

void InitFloppy(void) {
    floppy_lock = CreateMutex("floppy");

    CreateThread(FloppyMotorControlThread, NULL, GetVas(), "flpmotor");

    for (int i = 0; i < 1; ++i) {
        struct vnode* node = CreateVnode(dev_ops, (struct stat) {
            .st_mode = S_IFBLK | S_IRWXU | S_IRWXG | S_IRWXO,
            .st_nlink = 1,
            .st_blksize = 512,
            .st_blocks = 2880,
            .st_size = 512 * 2880
        });

        struct floppy_data* flp = AllocHeap(sizeof(struct floppy_data));
        *flp = (struct floppy_data) {
            .disk_num = i, .base = 0x3F0, 
            .stored_cylinder = -1, .got_cylinder_zero = false,
            .cylinder_buffer = (uint8_t*) MapVirt(0, 0, CYLINDER_SIZE, VM_READ | VM_WRITE | VM_LOCK, NULL, 0),
            .cylinder_zero   = (uint8_t*) MapVirt(0, 0, CYLINDER_SIZE, VM_READ | VM_WRITE | VM_LOCK, NULL, 0),
        };
        node->data = flp;

        RegisterIrqHandler(PIC_IRQ_BASE + 6, FloppyIrqHandler);
        FloppyReset(flp);

        InitDiskPartitionHelper(&flp->partitions);

        AddVfsMount(node, GenerateNewRawDiskName(DISKUTIL_TYPE_FLOPPY));
        CreateDiskPartitions(CreateOpenFile(node, 0, 0, true, true));
    }
}
