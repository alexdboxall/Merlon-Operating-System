#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include <vfs.h>
#include <transfer.h>
#include <heap.h>
#include <errno.h>
#include <semaphore.h>
#include <log.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

static struct semaphore* mounting_mutex = NULL;

static struct open_file* disks[FF_VOLUMES] = {0};
static int disk_sector_sizes[FF_VOLUMES] = {0};
static int disk_sector_counts[FF_VOLUMES] = {0};
static uint8_t next_fatfs_volume = 0;

static struct semaphore* mutexes[FF_VOLUMES + 1];

DSTATUS disk_status(BYTE pdrv) {
	(void) pdrv;
	return 0;
}

DSTATUS disk_initialize(BYTE pdrv) {
	(void) pdrv;
	return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
	if (pdrv >= FF_VOLUMES || disks[pdrv] == NULL || buff == NULL) {
		return RES_PARERR;
	}

	struct transfer io = CreateKernelTransfer(buff, count * disk_sector_sizes[pdrv], sector * disk_sector_sizes[pdrv], TRANSFER_READ);
	int res = ReadFile(disks[pdrv], &io);
	if (res != 0) {
		return RES_ERROR;
	}

	return 0;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
	if (pdrv >= FF_VOLUMES || disks[pdrv] == NULL || buff == NULL) {
		return RES_PARERR;
	}

	struct transfer io = CreateKernelTransfer((void*) buff, count * disk_sector_sizes[pdrv], sector * disk_sector_sizes[pdrv], TRANSFER_WRITE);
	int res = WriteFile(disks[pdrv], &io);
	if (res == EROFS) {
		return RES_WRPRT;

	} else if (res != 0) {
		return RES_ERROR;
	}

	return 0;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
	if (pdrv >= FF_VOLUMES || disks[pdrv] == NULL) {
		return RES_PARERR;
	}

	switch (cmd) {
	case CTRL_SYNC:
		return RES_OK;

	case GET_SECTOR_COUNT: 
		if (buff == NULL) return RES_PARERR;
		*((LBA_t*) buff) = disk_sector_counts[pdrv];
		return RES_OK;

	case GET_SECTOR_SIZE:
		if (buff == NULL) return RES_PARERR;
		*((WORD*) buff) = disk_sector_sizes[pdrv];
		return RES_OK;

	case GET_BLOCK_SIZE:
		if (buff == NULL) return RES_PARERR;
		*((DWORD*) buff) = 1;
		return RES_OK;

	case CTRL_TRIM:
		return RES_ERROR;
	
	default:
		return RES_PARERR;
	}
}

void* ff_memalloc(UINT msize) {
	LogWriteSerial("ff_memalloc 0x%X\n", msize);
	/*
	 * The heap is only used for LFN stuff - and this doesn't occur on f_read or f_write.
	 * These are the only two that would be called from a page fault, and therefore it's okay for
	 * this data to be paged out.
	 */
	return AllocHeapEx((size_t) msize, HEAP_ALLOW_PAGING);
}

void ff_memfree(void* mblock) {
	FreeHeap(mblock);
}

int ff_mutex_create(int vol) {
	mutexes[vol] = CreateMutex("fatfs");
	return 1;
}

void ff_mutex_delete(int vol)
{
	DestroyMutex(mutexes[vol], SEM_DONT_CARE);
}

int ff_mutex_take(int vol) {
	int res = AcquireMutex(mutexes[vol], FF_FS_TIMEOUT);
	if (res == ETIMEDOUT || res == EAGAIN) {
		return 0;
	}
	return 1;
}

void ff_mutex_give(int vol) {
	ReleaseMutex(mutexes[vol]);
}

struct vnode_data {
	FATFS* fatfs_drive;
	FIL* fatfs_file;
	FATDIR* fatfs_dir;
	int disk_num;
};

static int CheckOpen(struct vnode*, const char* name, int) {
    if (strlen(name) >= FF_LFN_BUF) {
        return ENAMETOOLONG;
	}

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

#define TRANSFER_CHUNK_SIZE 1024

static int Read(struct vnode* node, struct transfer* io) {    
    struct vnode_data* data = node->data;

	uint8_t buffer[TRANSFER_CHUNK_SIZE];
	while (io->length_remaining > 0) {
		int amount = io->length_remaining > TRANSFER_CHUNK_SIZE ? TRANSFER_CHUNK_SIZE : io->length_remaining;
		UINT br;
		FRESULT fres = f_read(data->fatfs_file, buffer, amount, &br);
		if (fres == FR_DISK_ERR) {
			return EIO;
		} else if (fres == FR_INT_ERR) {
			return EIO;
		} else if (fres == FR_DENIED) {
			return EINVAL;
		} else if (fres == FR_INVALID_OBJECT) {
			return EINVAL;
		} else if (fres == FR_TIMEOUT) {
			return ETIMEDOUT;
		} else if (fres != 0) {
			return EIO;
		}

		int kres = PerformTransfer(buffer, io, br);
		if (kres != 0) {
			return kres;
		}
	}

	return 0;
}

static int Readdir(struct vnode*, struct transfer*) {
    return ENOSYS;
}

static int Write(struct vnode* node, struct transfer* io) {
    struct vnode_data* data = node->data;

	uint8_t buffer[TRANSFER_CHUNK_SIZE];
	while (io->length_remaining > 0) {
		UINT amount = io->length_remaining > TRANSFER_CHUNK_SIZE ? TRANSFER_CHUNK_SIZE : io->length_remaining;

		int kres = PerformTransfer(buffer, io, amount);
		if (kres != 0) {
			return kres;
		}

		UINT br;
		FRESULT fres = f_write(data->fatfs_file, buffer, amount, &br);
		if (fres == FR_DISK_ERR) {
			return EIO;
		} else if (fres == FR_WRITE_PROTECTED) {
			return EROFS;
		} else if (fres == FR_INT_ERR) {
			return EIO;
		} else if (fres == FR_DENIED) {
			return EINVAL;
		} else if (fres == FR_INVALID_OBJECT) {
			return EINVAL;
		} else if (fres == FR_TIMEOUT) {
			return ETIMEDOUT;
		} else if (fres != 0) {
			return EIO;
		}

		if (br != amount) {
			return EIO;
		}
	}

	return 0;
}

static int Create(struct vnode*, struct vnode**, const char*, int, mode_t) {
    return EROFS;
}

static uint8_t DirentType(struct vnode*) {
    return DT_UNKNOWN;
}

static int Stat(struct vnode*, struct stat*) {
    return ENOSYS;
}

static int Truncate(struct vnode*, off_t) {
    return EROFS;
}


static int Close(struct vnode* node) {
    struct vnode_data* data = node->data;
	FreeHeap(data->fatfs_drive);
    FreeHeap(data);
    return 0;
}

static struct vnode* CreateFatFsVnode();

static int Follow(struct vnode*, struct vnode**, const char*) {
    return ENOSYS;
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

static struct vnode* CreateFatFsVnode() {
    return CreateVnode(dev_ops);
}

int FatFsMountCreator(struct open_file* raw_device, struct open_file** out) { 
	if (mounting_mutex == NULL) {
		mounting_mutex = CreateMutex("fatfsmnt");
	}  

	struct vnode* node = CreateFatFsVnode();
    struct vnode_data* data = AllocHeap(sizeof(struct vnode_data));
    
    data->fatfs_drive = AllocHeapEx(sizeof(FATFS), HEAP_ALLOW_PAGING | HEAP_FORCE_PAGING);
    data->fatfs_file = NULL;
    data->fatfs_dir = NULL;
    node->data = data;

	*out = CreateOpenFile(node, 0, 0, true, false);

	int mtxres = AcquireMutex(mounting_mutex, 2500);
	if (mtxres != 0) {
		return ENOTSUP;
	}

	uint8_t id = next_fatfs_volume;

	data->disk_num = id;

	struct stat st;
	VnodeOpStat(raw_device->node, &st);

	disks[id] = raw_device;
	disk_sector_counts[id] = st.st_blocks;
	disk_sector_sizes[id] = st.st_blksize;

	char path[3];
	path[0] = '0' + id;
	path[1] = ':';
	path[2] = 0;

	FRESULT res = f_mount(data->fatfs_drive, (const TCHAR*) path, 1);
	if (res != 0) {
		disks[id] = NULL;
		CloseFile(*out);
		*out = NULL;
		ReleaseMutex(mounting_mutex);
		return ENOTSUP;
	}

	next_fatfs_volume++;
	ReleaseMutex(mounting_mutex);

    return 0;
}