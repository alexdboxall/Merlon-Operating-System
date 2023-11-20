#include <fs/internal/fat.h>
#include <errno.h>
#include <assert.h>

/*
 * You are really going to want to write a disk cache at some point. WriteFatEntry is brutal.
 */

static int AccessFatSectors(struct fat_data* fat, int sector, uint8_t* buffer, int num_sectors, bool write) {
    (void) fat;
    (void) sector;
    (void) buffer;
    (void) num_sectors;
    (void) write;

    return (sector * num_sectors ^ (size_t) buffer) + write;
}

static int ClusterToSector(struct fat_data* fat, int cluster) {
    return (cluster - 2) * fat->sectors_per_cluster + fat->first_data_sector;
}

int ReadFatCluster(struct fat_data* fat, int cluster, bool buffer) {
    return AccessFatSectors(fat, ClusterToSector(fat, cluster), buffer ? fat->cluster_buffer_b : fat->cluster_buffer_a, fat->sectors_per_cluster, false);
}

int WriteFatCluster(struct fat_data* fat, int cluster, bool buffer) {
    return AccessFatSectors(fat, ClusterToSector(fat, cluster), buffer ? fat->cluster_buffer_b : fat->cluster_buffer_a, fat->sectors_per_cluster, true);
}

int ReadFatEntry(struct fat_data* fat, int entry, uint32_t* output) {
    assert(FAT16 == 2);
    assert(FAT32 == 4);

    if (fat->fat_type == FAT16 || fat->fat_type == FAT32) {
        int sector = (entry * fat->fat_type) / fat->bytes_per_sector + fat->first_fat_sector;
        int offset = (entry * fat->fat_type) % fat->bytes_per_sector;
        uint8_t* buffer = fat->cluster_buffer_a;

        int status = AccessFatSectors(fat, sector, buffer, 1, false);
        if (status != 0) {
            return status;
        }

        uint32_t result = buffer[offset + 0] | (buffer[offset + 1] << 8);
        if (fat->fat_type == FAT32) {
            result |= (buffer[offset + 2] << 16) | (buffer[offset + 3] << 24);
        }
        *output = result;
        return 0;

    } else {
        int absolute_offset = (entry + entry / 2) + fat->bytes_per_sector * fat->first_fat_sector;
        int sector = absolute_offset / fat->bytes_per_sector;
        int offset = absolute_offset % fat->bytes_per_sector;
        bool straddles = ((absolute_offset + 1) / fat->bytes_per_sector) != sector;
        uint8_t* buffer = fat->cluster_buffer_a;

        int status = AccessFatSectors(fat, sector, buffer, straddles ? 1 : 2, false);
        if (status != 0) {
            return status;
        }

        if (offset & 1) {
            *output = (buffer[offset] >> 4) | (buffer[offset + 1] << 4);
        } else {
            *output = buffer[offset] | ((buffer[offset + 1] & 0xF) << 8);
        }

        return 0;
    }
}

int WriteFatEntry(struct fat_data* fat, int entry, uint32_t value) {
    if (fat->fat_type == FAT16 || fat->fat_type == FAT32) {
        int sector = (entry * fat->fat_type) / fat->bytes_per_sector + fat->first_fat_sector;
        int offset = (entry * fat->fat_type) % fat->bytes_per_sector;
        uint8_t* buffer = fat->cluster_buffer_a;

        int status = AccessFatSectors(fat, sector, buffer, 1, false);
        if (status != 0) {
            return status;
        }

        buffer[offset + 0] = (value >> 0) & 0xFF;
        buffer[offset + 1] = (value >> 8) & 0xFF;
        if (fat->fat_type == FAT32) {
            buffer[offset + 2] = (value >> 16) & 0xFF;
            buffer[offset + 3] = (value >> 24) & 0xFF;
        }

        for (int i = 0; i < fat->num_fats; ++i) {
            status = AccessFatSectors(fat, sector, buffer, 1, true);
            if (status != 0) {
                return status;
            }
            sector += fat->sectors_per_fat;
        }

        return 0;

    } else {
        int absolute_offset = (entry + entry / 2) + fat->bytes_per_sector * fat->first_fat_sector;
        int sector = absolute_offset / fat->bytes_per_sector;
        int offset = absolute_offset % fat->bytes_per_sector;
        bool straddles = ((absolute_offset + 1) / fat->bytes_per_sector) != sector;
        uint8_t* buffer = fat->cluster_buffer_a;

        int status = AccessFatSectors(fat, sector, buffer, straddles ? 1 : 2, false);
        if (status != 0) {
            return status;
        }
        
        if (offset & 1) {
            buffer[offset] &= 0x0F;
            buffer[offset] |= (value & 0xF) << 4;
            buffer[offset + 1] = value >> 4;
        } else {
            buffer[offset] = value & 0xFF;
            buffer[offset + 1] &= 0xF0;
            buffer[offset + 1] |= value >> 8;
        }

        for (int i = 0; i < fat->num_fats; ++i) {
            status = AccessFatSectors(fat, sector, buffer, straddles ? 1 : 2, false);
            if (status != 0) {
                return status;
            }
            sector += fat->sectors_per_fat;
        }

        return 0;
    }
}