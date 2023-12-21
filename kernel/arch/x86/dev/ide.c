#include <common.h>
#include <semaphore.h>
#include <log.h>
#include <thread.h>
#include <vfs.h>
#include <transfer.h>
#include <assert.h>
#include <errno.h>
#include <machine/portio.h>
#include <heap.h>
#include <virtual.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <irql.h>
#include <diskutil.h>

#define MAX_TRANSFER_SIZE (1024 * 16)

struct semaphore* ide_lock = NULL;

struct ide_data {
    int disk_num;
    unsigned int sector_size;
    uint64_t total_num_sectors;
    uint16_t* transfer_buffer;
    size_t primary_base;
    size_t primary_alternative;
    size_t secondary_base;
    size_t secondary_alternative;
    size_t busmaster_base;

    struct disk_partition_helper partitions;
};

int IdeCheckError(struct ide_data* ide) {
    uint16_t base = ide->disk_num >= 2 ? ide->secondary_base : ide->primary_base;

    uint8_t status = inb(base + 0x7);
    if (status & 0x01) {
        LogDeveloperWarning("[ide] unknown error\n");
        return EIO;

    } else if (status & 0x20) {
        LogDeveloperWarning("[ide] driver write fault\n");
        return EIO;

    } else if (!(status & 0x08)) {
        LogDeveloperWarning("[ide] meant to be ready for data, but isn't\n");
        return EIO;
    }

    return 0;
}

int IdePoll(struct ide_data* ide) {
    uint16_t base = ide->disk_num >= 2 ? ide->secondary_base : ide->primary_base;
    uint16_t alt_status_reg = ide->disk_num >= 2 ? ide->secondary_alternative : ide->primary_alternative;

    /*
    * Delay for a moment by reading the alternate status register.
    */
    for (int i = 0; i < 4; ++i) {
        inb(alt_status_reg);
    }

    /*
    * Wait for the device to not be busy. We have a timeout in case the
    * device is faulty, we don't want to be in an endless loop and freeze
    * the kernel.
    */
    int timeout = 0;
    while (inb(base + 0x7) & 0x80) {
        if (timeout > 9975) {
            /*
            * For the last 25 iterations, wait 10ms
            */
            SleepMilli(10);
        }
        if (timeout++ > 10000) {
            return EIO;
        }
    }

    return 0;
}

/*
* Read or write the primary ATA drive on the first controller. We use LBA28, 
* so we are limited to a 28 bit sector number (i.e. disks up to 128GB in size)
*/
static int IdeIo(struct ide_data* ide, struct transfer* io) {
    int disk_num = ide->disk_num;
    
    /*
    * IDE devices do not contain an (accessible) disk buffer in PIO mode, as
    * they transfer data through the IO ports. Hence we must read/write into
    * this buffer first, and then move it safely to the destination. 
    * (we could use DMA instead, but PIO is simpler)
    * 
    * Allow up to 4KB sector sizes. Make sure there is enough room on the
    * stack to handle this.
    */
    uint16_t* buffer = ide->transfer_buffer;

    int sector = io->offset / ide->sector_size;
    int count = io->length_remaining / ide->sector_size;

    if (io->offset % ide->sector_size != 0) {
        return EINVAL;
    }
    if (io->length_remaining % ide->sector_size != 0) {
        return EINVAL;
    }
    if (count <= 0 || sector < 0 || sector > 0xFFFFFFF || (uint64_t) sector + count >= ide->total_num_sectors) {
        return EINVAL;
    }

    AcquireSemaphore(ide_lock, -1);

    uint16_t base = disk_num >= 2 ? ide->secondary_base : ide->primary_base;
    uint16_t dev_ctrl_reg = disk_num >= 2 ? ide->secondary_alternative : ide->primary_alternative;

    int max_sectors_at_once = MAX_TRANSFER_SIZE / ide->sector_size;
    if (max_sectors_at_once > 255) {
        // hardware limitation
        max_sectors_at_once = 255;
    }

    while (count > 0) {
        int sectors_in_this_transfer = count > max_sectors_at_once ? max_sectors_at_once : count;

        if (io->direction == TRANSFER_WRITE) {
            PerformTransfer(buffer, io, ide->sector_size);
        }

        /*
        * Send a whole heap of flags and the high 4 bits of the LBA to the controller.
        */
        outb(base + 0x6, 0xE0 | ((disk_num & 1) << 4) | ((sector >> 24) & 0xF));

        /*
        * Disable interrupts, we are going to use polling.
        */
        outb(dev_ctrl_reg, 2);

        /*
        * May not be needed, but it doesn't hurt to do it.
        */
        outb(base + 0x1, 0x00);

        /*
        * Send the number of sectors, and the sector's LBA.
        */
        outb(base + 0x2, sectors_in_this_transfer);
        outb(base + 0x3, (sector >> 0) & 0xFF);
        outb(base + 0x4, (sector >> 8) & 0xFF);
        outb(base + 0x5, (sector >> 16) & 0xFF);

        /*
        * Send either the read or write command.
        */
        outb(base + 0x7, io->direction == TRANSFER_WRITE ? 0x30 : 0x20);

        /*
        * Wait for the data to be ready.
        */
        IdePoll(ide);

        /*
        * Read/write the data from/to the disk using ports.
        */
        if (io->direction == TRANSFER_WRITE) {
            for (int c = 0; c < sectors_in_this_transfer; ++c) {
                if (c != 0) {
                    PerformTransfer(buffer, io, ide->sector_size);
                    IdePoll(ide);
                }
                
                for (uint64_t i = 0; i < ide->sector_size / 2; ++i) {
                    outw(base + 0x00, buffer[i]);
                }
            }

            /*
            * We need to flush the disk's cache if we are writing.
            */
            outb(base + 0x7, 0xE7);
            IdePoll(ide);

        } else {
            int err = IdeCheckError(ide);
            if (err) {
                ReleaseSemaphore(ide_lock);
                return err;
            }
             
            for (int c = 0; c < sectors_in_this_transfer; ++c) {
                if (c != 0) {
                    IdePoll(ide);
                }

                for (uint64_t i = 0; i < ide->sector_size / 2; ++i) {
                    buffer[i] = inw(base + 0x00);
                }

                PerformTransfer(buffer, io, ide->sector_size);
            }
        }

        /*
        * Get ready for the next part of the transfer.
        */
        count -= sectors_in_this_transfer;
        sector += sectors_in_this_transfer;
    }

    ReleaseSemaphore(ide_lock);

    return 0;
}

static int IdeGetNumSectors(struct ide_data* ide) {
    AcquireSemaphore(ide_lock, -1);

    uint16_t base = ide->disk_num >= 2 ? ide->secondary_base : ide->primary_base;

    /*
    * Select the correct drive.
    */
    outb(base + 0x6, 0xE0 | ((ide->disk_num & 1) << 4));

    /*
    * Send the READ NATIVE MAX ADDRESS command, which will return the size
    * of disk in sectors.
    */
    outb(base + 0x7, 0xF8);

    IdePoll(ide);

    /*
    * The outputs are in the same registers we use to put the LBA
    * when we read/write from the disk.
    */
    int sectors = 0;
    sectors |= (int) inb(base + 0x3);
    sectors |= ((int) inb(base + 0x4)) << 8;
    sectors |= ((int) inb(base + 0x5)) << 16;
    sectors |= ((int) inb(base + 0x6) & 0xF) << 24;

    ReleaseSemaphore(ide_lock);

    return sectors;
}


static int CheckOpen(struct vnode*, const char*, int) {
    return 0;
}

static int Ioctl(struct vnode*, int, void*) {
    return EINVAL;
}

static bool IsSeekable(struct vnode*) {
    return true;
}

static int IsTty(struct vnode*) {
    return false;
}

static int Read(struct vnode* node, struct transfer* io) { 
    struct ide_data* ide = node->data;  
    if (io->direction != TRANSFER_READ) {
        return EINVAL;
    } 
    return IdeIo(ide, io);
}

static int Write(struct vnode* node, struct transfer* io) {
    struct ide_data* ide = node->data;  
    if (io->direction != TRANSFER_WRITE) {
        return EINVAL;
    } 
    return IdeIo(ide, io);
}

static int Create(struct vnode* node, struct vnode** partition, const char* name, int, mode_t) {
    AcquireSemaphore(ide_lock, -1);
    struct ide_data* ide = node->data;
    int res = DiskCreateHelper(&ide->partitions, partition, name);
    ReleaseSemaphore(ide_lock);
    return res;
}

static int Follow(struct vnode* node, struct vnode** output, const char* name) {
    AcquireSemaphore(ide_lock, -1);
    struct ide_data* ide = node->data;
    int res = DiskFollowHelper(&ide->partitions, output, name);
    ReleaseSemaphore(ide_lock);
    return res;
}


static uint8_t DirentType(struct vnode*) {
    return DT_BLK;
}

static int Stat(struct vnode* node, struct stat* st) {
    struct ide_data* ide = node->data;  

    st->st_mode = S_IFCHR | S_IRWXU | S_IRWXG | S_IRWXO;
    st->st_atime = 0;
    st->st_blksize = ide->sector_size;
    st->st_blocks = ide->total_num_sectors;
    st->st_ctime = 0;
    st->st_dev = 0xBABECAFE;
    st->st_gid = 0;
    st->st_ino = 0xCAFEBABE;
    st->st_mtime = 0;
    st->st_nlink = 1;
    st->st_rdev = 0xCAFEDEAD;
    st->st_size = ide->sector_size * ide->total_num_sectors;
    st->st_uid = 0;

    return 0;
}

static int Truncate(struct vnode*, off_t) {
    return EINVAL;
}

static int Close(struct vnode*) {
    return 0;
}

static int Readdir(struct vnode*, struct transfer*) {
    return EINVAL;
}

static const struct vnode_operations dev_ops = {
    .check_open     = CheckOpen,
    .ioctl          = Ioctl,
    .is_seekable    = IsSeekable,
    .is_tty         = IsTty,
    .read           = Read,
    .write          = Write,
    .close          = Close,
    .truncate       = Truncate,
    .create         = Create,
    .follow         = Follow,
    .dirent_type    = DirentType,
    .readdir        = Readdir,
    .stat           = Stat,
};

void InitIde(void) {
    ide_lock = CreateMutex();

    for (int i = 0; i < 1; ++i) {
        struct vnode* node = CreateVnode(dev_ops);
        struct ide_data* ide = AllocHeap(sizeof(struct ide_data));

        ide->disk_num               = 0;
        ide->primary_base           = 0x1F0;
        ide->secondary_base         = 0x170;
        ide->primary_alternative    = 0x3F6;
        ide->secondary_alternative  = 0x376;
        ide->busmaster_base         = 0x0;
        ide->sector_size            = 512;
        ide->total_num_sectors      = IdeGetNumSectors(ide);
        ide->transfer_buffer        = (uint16_t*) MapVirt(0, 0, MAX_TRANSFER_SIZE, VM_READ | VM_WRITE | VM_LOCK, NULL, 0);
        InitDiskPartitionHelper(&ide->partitions);

        node->data = ide;
        AddVfsMount(node, GenerateNewRawDiskName(DISKUTIL_TYPE_FIXED));
        CreateDiskPartitions(CreateOpenFile(node, 0, 0, true, true));
    }
}
