
#ifndef RAIDVOL_H
#define RAIDVOL_H

#include <stdint.h>
#include <stddef.h>

#define RAIDVOL_BLOCK_SIZE 512
#define RAIDVOL_MAX_DEVICES 8

typedef enum {
    RAID0 = 0,
    RAID1 = 1,
    RAID5 = 5
} RaidLevel;

typedef struct {
    int fd[RAIDVOL_MAX_DEVICES];
int metadata_fd;
    int num_devices;
    RaidLevel level;
    uint32_t stripe_blocks;
    uint64_t device_blocks;
    uint64_t virtual_blocks;
    int failed_device;
    uint32_t *checksums;
    unsigned char *checksum_valid;
} RaidVolume;

int raidvol_create(
    RaidVolume *v,
    RaidLevel level,
    const char **paths,
    int num_devices,
    uint64_t device_blocks,
    uint32_t stripe_blocks
);

int raidvol_open_existing(
    RaidVolume *v,
    RaidLevel level,
    const char **paths,
    int num_devices,
    uint64_t device_blocks,
    uint32_t stripe_blocks
);
int raidvol_write_block(
    RaidVolume *v,
    uint64_t lba,
    const void *buffer
);

int raidvol_read_block(
    RaidVolume *v,
    uint64_t lba,
    void *buffer
);

int raidvol_rebuild(
    RaidVolume *v,
    int failed_device,
    const char *replacement_path
);

void raidvol_close(RaidVolume *v);

#endif
