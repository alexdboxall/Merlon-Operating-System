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

    LogWriteSerial("floppy_write_cmd: timeout\n");
}

static uint8_t FloppyReadData(struct floppy_data* flp) {
    int base = flp->base;
    for (int i = 0; i < 60; ++i) {
        SleepMilli(10);
        if (inb(base + FLOPPY_MSR) & 0x80) {
            return inb(base + FLOPPY_FIFO);
        }
    }

    LogWriteSerial("floppy_read_data: timeout\n");
    return 0;
}

static void FloppyCheckInterrupt(struct floppy_data* flp, int* st0, int* cyl) {
    LogWriteSerial("FloppyCheckInterrupt\n");

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
    LogWriteSerial("FloppyMotor\n");

    int base = flp->base;

    if (state) {
        if (!floppy_motor_state) {
            outb(base + FLOPPY_DOR, 0x1C);
            SleepMilli(150);
        }
        floppy_motor_state = 1;

    } else {
        floppy_motor_state = 2;
        floppy_motor_ticks = 1000;
    }
}

static volatile bool floppy_got_irq = false;

static void FloppyIrqWait() {
    LogWriteSerial("FloppyIrqWait\n");

    /*
    * Wait for the interrupt to come. If it doesn't the system will probably
    * lockup.
    */
    while (!floppy_got_irq) {
        SleepMilli(10);
    }

    /*
    * Clear it for next time.
    */
    floppy_got_irq = false;
}

static int FloppyIrqHandler(struct x86_regs*) {
    LogWriteSerial("FloppyIrqHandler\n");

    floppy_got_irq = true;
    return 0;
}

static void FloppyMotorControlThread(void*) {
    LogWriteSerial("FloppyMotorControlThread\n");

    while (1) {
        SleepMilli(50);
        if (floppy_motor_state == 2) {
            floppy_motor_ticks -= 50;
            if (floppy_motor_ticks <= 0) {
                /*
                * Actually turn off the motor.
                */
                //outb(0x3F0 + FLOPPY_DOR, 0x0C);
                //loppy_motor_state = 0;
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

        FloppyIrqWait();
        FloppyCheckInterrupt(flp, &st0, &cyl);

        if (st0 & 0xC0) {
            continue;
        }

        if (cyl == 0) {
            FloppyMotor(flp, false);
            return 0;
        }
    }

    LogDeveloperWarning("couldn't calibrate floppy\n");
    FloppyMotor(flp, false);
    return EIO;
}

static void FloppyConfigure(struct floppy_data* flp) {
    LogWriteSerial("FloppyConfigure\n");

    FloppyWriteCommand(flp, CMD_CONFIGURE);
    FloppyWriteCommand(flp, 0x00);
    FloppyWriteCommand(flp, 0x08);
    FloppyWriteCommand(flp, 0x00);
}

static int FloppyReset(struct floppy_data* flp) {
    LogWriteSerial("FloppyReset\n");

    int base = flp->base;
    outb(base + FLOPPY_DOR, 0x00);
    outb(base + FLOPPY_DOR, 0x0C);

    FloppyIrqWait();

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

    int res = FloppyCalibrate(flp);
    LogWriteSerial("FloppyReset: res = %d\n", res);
    return res;
}

static int FloppySeek(struct floppy_data* flp, int cylinder, int head) {
    LogWriteSerial("FloppySeek\n");

    int st0, cyl;
    FloppyMotor(flp, true);

    for (int i = 0; i < 10; ++i) {
        FloppyWriteCommand(flp, CMD_SEEK);
        FloppyWriteCommand(flp, head << 2);
        FloppyWriteCommand(flp, cylinder);

        FloppyIrqWait();
        FloppyCheckInterrupt(flp, &st0, &cyl);

        if (st0 & 0xC0) {
            continue;
        }

        if (cyl == cylinder) {
            FloppyMotor(flp, false);
            return 0;
        }
    }

    LogWriteSerial("couldn't seek floppy\n");
    FloppyMotor(flp, false);
    return EIO;
}

static void FloppyDmaInit(void) {
    LogWriteSerial("FloppyDmaInit\n");

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
    LogWriteSerial("FloppyDoCylinder\n");

    /*
    * Move both heads to the correct cylinder.
    */
    if (FloppySeek(flp, cylinder, 0) != 0) return EIO;
    if (FloppySeek(flp, cylinder, 1) != 0) return EIO;

    /*
    * This time, we'll try up to 20 times.
    */
    for (int i = 0; i < 20; ++i) {
        LogWriteSerial("READ ATTEMPT %d\n", i + 1);
        FloppyMotor(flp, true);

        if (i % 5 == 3) {
            if (i % 10 == 8) {
                LogWriteSerial("resetting floppy!\n");
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

        FloppyIrqWait();

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
            static const char * status[] = { 0, "error", "invalid command", "drive not ready" };
            LogWriteSerial("floppy_do_sector: status = %s\n", status[st0 >> 6]);
            LogWriteSerial("st0 = 0x%X, st1 = 0x%X, st2 = 0x%X, bps = %d\n", st0, st1, st2, bps); 
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

    LogWriteSerial("couldn't read floppy\n");
    FloppyMotor(flp, false);
    return EIO;
}



static int FloppyIo(struct floppy_data* flp, struct transfer* io) {
    LogWriteSerial("FloppyIo\n");
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
    * Floppies use CHS (cylinder, head, sector) for addressing sectors instead of 
    * LBA (linear block addressing). Hence we need to convert to CHS.
    * 
    * Head: which side of the disk it is on (i.e. either the top or bottom)
    * Cylinder: which 'slice' (cylinder) of the disk we should look at
    * Sector: which 'ring' (sector) of that cylinder we should look at
    * 
    * Note that sector is 1-based, whereas cylinder and head are 0-based.
    * Don't ask why.
    */
    int head = (lba % (18 * 2)) / 18;
    int cylinder = (lba / (18 * 2));
    int sector = (lba % 18) + 1;

    /*
    * Cylinder 0 has some commonly used data, so cache it seperately for improved
    * speed.
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
    * Read only read one sector, so we need to repeat the process if multiple sectors
    * were requested. We could just do a larger copy above, but this is a bit simpler,
    * as we don't need to worry about whether the entire request is on cylinder or not.
    */
    if (count > 0) {
        ++lba;
        --count;
        goto next_sector;
    }

    ReleaseMutex(floppy_lock);
    return 0;
}

static bool IsSeekable(struct vnode*) {
    return true;
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

static int Stat(struct vnode*, struct stat* st) {
    st->st_mode = S_IFBLK | S_IRWXU | S_IRWXG | S_IRWXO;
    st->st_atime = 0;
    st->st_blksize = 512;
    st->st_blocks = 2880;
    st->st_ctime = 0;
    st->st_dev = 0xBABECAFE;
    st->st_gid = 0;
    st->st_ino = 0xCAFEBABE;
    st->st_mtime = 0;
    st->st_nlink = 1;
    st->st_rdev = 0xCAFEDEAD;
    st->st_size = 1024 * 1440;
    st->st_uid = 0;
    return 0;
}

static int Close(struct vnode*) {
    return 0;
}

static const struct vnode_operations dev_ops = {
    .is_seekable    = IsSeekable,
    .read           = ReadWrite,
    .write          = ReadWrite,
    .close          = Close,
    .create         = Create,
    .follow         = Follow,
    .stat           = Stat,
};

void InitFloppy(void) {
    floppy_lock = CreateMutex("floppy");

    CreateThread(FloppyMotorControlThread, NULL, GetVas(), "flpmotor");

    for (int i = 0; i < 1; ++i) {
        struct vnode* node = CreateVnode(dev_ops);
        struct floppy_data* flp = AllocHeap(sizeof(struct floppy_data));

        RegisterIrqHandler(PIC_IRQ_BASE + 6, FloppyIrqHandler);

        flp->disk_num          = 0;
        flp->base              = 0x3F0;
        flp->cylinder_buffer   = (uint8_t*) MapVirt(0, 0, CYLINDER_SIZE, VM_READ | VM_WRITE | VM_LOCK, NULL, 0);
        flp->cylinder_zero     = (uint8_t*) MapVirt(0, 0, CYLINDER_SIZE, VM_READ | VM_WRITE | VM_LOCK, NULL, 0);
        flp->got_cylinder_zero = false;
        flp->stored_cylinder   = -1;
        FloppyReset(flp);

        InitDiskPartitionHelper(&flp->partitions);

        node->data = flp;
        AddVfsMount(node, GenerateNewRawDiskName(DISKUTIL_TYPE_FLOPPY));
        CreateDiskPartitions(CreateOpenFile(node, 0, 0, true, true));
    }
}
