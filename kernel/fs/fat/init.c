
#include <fs/internal/fat.h>
#include <string.h>
#include <errno.h>

int DetectFatPartition(void* partition) {
    (void) partition;

    uint8_t buffer[512];
    /* TODO: raw disk read into the buffer instead of calling memset */
    inline_memset(buffer, 0, 512);

    /*
     * TODO: we'd want to check the partition data to see if there is a filesystem ID.
     */

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
    - FAT can be detected by examining various fields and verifying that they are all valid:
    - Cluster size is a power of 2
    - Media type is 0xf0 or greater or equal to 0xf8
    - FAT size is not 0
    */

    /*
     * Check the OEM string.
     */
    for (int i = 3; i < 11; ++i) {
        if ((buffer[i] >= 1 && buffer[i] < 32) || buffer[i] > 127) {
            return ENOTSUP;
        }
    }

    // at this point, we have FAT!
    // check what type.

    struct fat_data fat;

    // TODO: load fat.total_clusters

    if (fat.total_clusters < 4084) {
        fat.fat_type = FAT12;
    } else if (fat.total_clusters < 65524) {
        fat.fat_type = FAT16;
    } else {
        fat.fat_type = FAT32;
    }

    // TODO: load the rest

    /*
    - Number of root directory entries is 0 (FAT32) or not 0 (FAT12, FAT16)
- Root cluster is valid (FAT32)
- File system version is 0 (FAT32)

    Check these and return ENOTSUP if not matching.
*/

    return ENOSYS;
}