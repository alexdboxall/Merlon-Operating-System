#pragma once

#include <common.h>

struct file;

#define LFN_SHORT_ONLY  0
#define LFN_BOTH        1
#define LFN_ERROR       2

#define FAT12   0
#define FAT16   2       // the value of 2 is relied on, as it means 2 bytes per FAT (is used for calcs)
#define FAT32   4       // as above, we use fat_type to do calcs, so required that FAT32 == 4

struct fat_data {
    int num_fats;
    int fat_sectors[4];
    int sectors_per_fat;
    union {
        uint64_t first_root_dir_sector_12_16;
        uint64_t root_dir_cluster_32;
    };
    uint64_t root_dir_num_sectors_12_16;
    int total_clusters;
    uint64_t first_data_sector;
    uint64_t first_fat_sector;
    int fat_type;               // FAT12 or FAT16 or FAT32
    int sectors_per_cluster;
    int bytes_per_sector;

    struct file* disk; // TODO! points to a vnode for the partition

    uint8_t* cluster_buffer_a;
    uint8_t* cluster_buffer_b;
};

int GetFatShortFilename(char* lfn, char* output, char* directory);
void FormatFatShortName(char* with_dot, char* without_dot);
void UnformatFatShortName(char* without_dot, char* with_dot);

int ReadFatCluster(struct fat_data* fat, int cluster, bool buffer);
int WriteFatCluster(struct fat_data* fat, int cluster, bool buffer);
int ReadFatEntry(struct fat_data* fat, int entry, uint32_t* output);
int WriteFatEntry(struct fat_data* fat, int entry, uint32_t value);

struct fat_data LoadFatData(uint8_t* boot_sector, struct file* disk);