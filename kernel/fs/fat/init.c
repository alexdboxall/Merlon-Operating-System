
#include <fs/internal/fat.h>
#include <string.h>
#include <errno.h>
#include <transfer.h>
#include <vfs.h>
#include <sys/stat.h>

int DetectFatPartition(struct open_file* disk) {
    struct stat st;
    int res = VnodeOpStat(disk->node, &st);
    if (res != 0) {
        return ENOTSUP;
    }

    uint8_t buffer[512];
    struct transfer io = CreateKernelTransfer(buffer, 512, 0, TRANSFER_READ);
    res = ReadFile(disk, &io);
    if (res != 0) {
        return ENOTSUP;
    }

    /*
     * Check that it's a boot sector at all.
     */
    if (buffer[0x1FE] != 0x55 || buffer[0x1FF] != 0xAA) {
        return ENOTSUP;
    }

    /*
     * Check that the number of FATs is sane (i.e. 1 or 2)
     */
    if (buffer[0x10] != 1 && buffer[0x10] != 2) {
        return ENOTSUP;
    }

    /*
     * Check that bytes-per-sector is a multiple of 256.
     */
    if (buffer[0x0B] != 0) {
        return ENOTSUP;
    }

    /*
     * Check for valid media type.
     */
    if ((buffer[0x15] >> 4) != 0xF) {
        return ENOTSUP;
    }

    /*
     * Check the OEM string has valid characters.
     */
    for (int i = 3; i < 11; ++i) {
        if ((buffer[i] >= 1 && buffer[i] < 32) || buffer[i] > 127) {
            return ENOTSUP;
        }
    }

    struct fat_data fat = LoadFatData(buffer, disk);
    (void) fat;


    // TODO: load the rest

    /*
    - Number of root directory entries is 0 (FAT32) or not 0 (FAT12, FAT16)
- Root cluster is valid (FAT32)
- File system version is 0 (FAT32)

    Check these and return ENOTSUP if not matching.
*/

    return 0;
}