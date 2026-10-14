
#define _POSIX_C_SOURCE 200809L

#include "raidvol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BLOCK_SIZE RAIDVOL_BLOCK_SIZE
static uint32_t calculate_checksum(const void *data)
{
    const unsigned char *bytes = data;
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}
static int verify_checksum(RaidVolume *v,
                           uint64_t lba,
                           const void *buffer)
{
    if (!v || !buffer || lba >= v->virtual_blocks)
        return -1;

    if (!v->checksum_valid || !v->checksums)
        return -1;

    if (!v->checksum_valid[lba])
        return 0;

    return calculate_checksum(buffer) == v->checksums[lba]
           ? 0 : -1;
}
static int io_full(int fd, void *buffer, size_t size,
                   uint64_t offset, int write_mode)
{
    unsigned char *p = buffer;
    size_t done = 0;

    while (done < size) {
        ssize_t n;

        if (offset + done > (uint64_t)INT64_MAX)
            return -1;

        if (write_mode)
            n = pwrite(fd, p + done, size - done,
                       (off_t)(offset + done));
        else
            n = pread(fd, p + done, size - done,
                      (off_t)(offset + done));

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return -1;

        done += (size_t)n;
    }

    return 0;
}

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t level;
    uint32_t num_devices;
    uint32_t stripe_blocks;
    uint64_t device_blocks;
    uint64_t virtual_blocks;
} RaidMetaHeader;

#define META_VERSION 1

static char *metadata_path(const char *path)
{
    const char suffix[] = ".raidvol.meta";
    size_t len = strlen(path) + sizeof(suffix);
    char *result = malloc(len);

    if (result != NULL) {
        snprintf(result, len, "%s%s", path, suffix);
    }

    return result;
}

static uint64_t metadata_checksums_offset(void)
{
    return sizeof(RaidMetaHeader);
}

static uint64_t metadata_valid_offset(const RaidVolume *v)
{
    return sizeof(RaidMetaHeader) +
           v->virtual_blocks * sizeof(uint32_t);
}

static int metadata_write_header(RaidVolume *v)
{
    RaidMetaHeader h = {0};

    memcpy(h.magic, "RAIDVOL1", 8);
    h.version = META_VERSION;
    h.level = (uint32_t)v->level;
    h.num_devices = (uint32_t)v->num_devices;
    h.stripe_blocks = v->stripe_blocks;
    h.device_blocks = v->device_blocks;
    h.virtual_blocks = v->virtual_blocks;

    return io_full(v->metadata_fd, &h, sizeof(h), 0, 1);
}

static int metadata_store_block(RaidVolume *v, uint64_t lba)
{
    if (!v || v->metadata_fd < 0 ||
        lba >= v->virtual_blocks)
        return -1;

    uint64_t checksum_offset =
        metadata_checksums_offset() +
        lba * sizeof(uint32_t);

    uint64_t valid_offset =
        metadata_valid_offset(v) + lba;

    if (io_full(v->metadata_fd, &v->checksums[lba],
                sizeof(uint32_t), checksum_offset, 1) < 0)
        return -1;

    if (io_full(v->metadata_fd, &v->checksum_valid[lba],
                sizeof(unsigned char), valid_offset, 1) < 0)
        return -1;

    return fsync(v->metadata_fd);
}
static int read_dev(RaidVolume *v, int dev, uint64_t block,
                    void *buffer)
{
    if (dev < 0 || dev >= v->num_devices ||
        v->fd[dev] < 0 || block >= v->device_blocks)
        return -1;

    return io_full(v->fd[dev], buffer, BLOCK_SIZE,
                   block * BLOCK_SIZE, 0);
}

static int write_dev(RaidVolume *v, int dev, uint64_t block,
                     const void *buffer)
{
    if (dev < 0 || dev >= v->num_devices ||
        v->fd[dev] < 0 || block >= v->device_blocks)
        return -1;

    return io_full(v->fd[dev], (void *)buffer, BLOCK_SIZE,
                   block * BLOCK_SIZE, 1);
}

static int raid5_map(RaidVolume *v, uint64_t lba,
                     int *data_dev, uint64_t *physical,
                     int *parity_dev)
{
    uint64_t data_disks = (uint64_t)v->num_devices - 1;
    uint64_t stripe_data = data_disks * v->stripe_blocks;
    uint64_t stripe = lba / stripe_data;
    uint64_t in_stripe = lba % stripe_data;
    uint64_t chunk = in_stripe / v->stripe_blocks;
    uint64_t offset = in_stripe % v->stripe_blocks;

    int parity = (int)(stripe % v->num_devices);
    int dev = (parity + 1 + (int)chunk) % v->num_devices;

    *data_dev = dev;
    *physical = stripe * v->stripe_blocks + offset;
    *parity_dev = parity;

    return 0;
}

int raidvol_create(RaidVolume *v, RaidLevel level,
                   const char **paths, int num_devices,
                   uint64_t device_blocks, uint32_t stripe_blocks)
{
    if (!v || !paths || num_devices < 1 ||
        num_devices > RAIDVOL_MAX_DEVICES ||
        device_blocks == 0 || stripe_blocks == 0)
        return -1;

    if (level != RAID0 && level != RAID1 && level != RAID5)
        return -1;

    if (level == RAID1 && num_devices < 2)
        return -1;

    if (level == RAID5 && num_devices < 3)
        return -1;

    if (device_blocks > (uint64_t)INT64_MAX / BLOCK_SIZE)
        return -1;

    if (level == RAID5 &&
        device_blocks % stripe_blocks != 0)
        return -1;

    memset(v, 0, sizeof(*v));

    for (int i = 0; i < RAIDVOL_MAX_DEVICES; i++)
        {
         v->fd[i] = -1;
        }
    v->metadata_fd = -1;
    v->num_devices = num_devices;
    v->level = level;
    v->stripe_blocks = stripe_blocks;
    v->device_blocks = device_blocks;
    v->failed_device = -1;

    if (level == RAID0) {
        v->virtual_blocks = device_blocks * (uint64_t)num_devices;
    } else if (level == RAID1) {
        v->virtual_blocks = device_blocks;
    } else {
        v->virtual_blocks =
            device_blocks * (uint64_t)(num_devices - 1);
    }
       if (v->virtual_blocks > SIZE_MAX / sizeof(uint32_t))
        return -1;

    v->checksums = calloc(
        (size_t)v->virtual_blocks, sizeof(uint32_t));

    v->checksum_valid = calloc(
        (size_t)v->virtual_blocks, sizeof(unsigned char));

    if (!v->checksums || !v->checksum_valid) {
        free(v->checksums);
        free(v->checksum_valid);
        v->checksums = NULL;
        v->checksum_valid = NULL;
        return -1;
    }
    for (int i = 0; i < num_devices; i++) {
        v->fd[i] = open(paths[i],
                        O_RDWR | O_CREAT | O_TRUNC,
                        0600);

        if (v->fd[i] < 0) {
            perror("open");
            raidvol_close(v);
            return -1;
        }

        off_t size = (off_t)(device_blocks * BLOCK_SIZE);

        if (ftruncate(v->fd[i], size) < 0) {
            perror("ftruncate");
            raidvol_close(v);
            return -1;
        }
    }
    char *meta_path = metadata_path(paths[0]);

    if (meta_path == NULL) {
        raidvol_close(v);
        return -1;
    }

    v->metadata_fd = open(
        meta_path, O_RDWR | O_CREAT | O_TRUNC, 0600);

    free(meta_path);

    if (v->metadata_fd < 0) {
        perror("metadata open");
        raidvol_close(v);
        return -1;
    }

    if (metadata_write_header(v) < 0) {
        perror("metadata header");
        raidvol_close(v);
        return -1;
    }
uint64_t metadata_size =
    sizeof(RaidMetaHeader) +
    v->virtual_blocks * sizeof(uint32_t) +
    v->virtual_blocks;

if (ftruncate(v->metadata_fd, (off_t)metadata_size) < 0) {
    perror("metadata ftruncate");
    raidvol_close(v);
    return -1;
}
    return 0;
}

int raidvol_open_existing(
    RaidVolume *v,
    RaidLevel level,
    const char **paths,
    int num_devices,
    uint64_t device_blocks,
    uint32_t stripe_blocks)
{
    if (!v || !paths || num_devices < 1 ||
        num_devices > RAIDVOL_MAX_DEVICES ||
        device_blocks == 0 || stripe_blocks == 0)
        return -1;

    if (level != RAID0 && level != RAID1 && level != RAID5)
        return -1;

    if (level == RAID1 && num_devices < 2)
        return -1;

    if (level == RAID5 &&
        (num_devices < 3 || device_blocks % stripe_blocks != 0))
        return -1;

    if (device_blocks > (uint64_t)INT64_MAX / BLOCK_SIZE)
        return -1;

    memset(v, 0, sizeof(*v));

    for (int i = 0; i < RAIDVOL_MAX_DEVICES; i++)
        v->fd[i] = -1;

    v->metadata_fd = -1;
    v->num_devices = num_devices;
    v->level = level;
    v->stripe_blocks = stripe_blocks;
    v->device_blocks = device_blocks;
    v->failed_device = -1;

    if (level == RAID0)
        v->virtual_blocks = device_blocks * (uint64_t)num_devices;
    else if (level == RAID1)
        v->virtual_blocks = device_blocks;
    else
        v->virtual_blocks = device_blocks * (uint64_t)(num_devices - 1);

    if (v->virtual_blocks > SIZE_MAX / sizeof(uint32_t))
        goto fail;

    v->checksums = calloc(
        (size_t)v->virtual_blocks, sizeof(uint32_t));
    v->checksum_valid = calloc(
        (size_t)v->virtual_blocks, sizeof(unsigned char));

    if (!v->checksums || !v->checksum_valid)
        goto fail;

    for (int i = 0; i < num_devices; i++) {
        if (!paths[i])
            goto fail;

        v->fd[i] = open(paths[i], O_RDWR);
        if (v->fd[i] < 0) {
            perror("open existing device");
            goto fail;
        }

        struct stat st;
        if (fstat(v->fd[i], &st) < 0 ||
            st.st_size < (off_t)(device_blocks * BLOCK_SIZE)) {
            fprintf(stderr, "Existing device file is too small\n");
            goto fail;
        }
    }

    char *meta_path = metadata_path(paths[0]);
    if (!meta_path)
        goto fail;

    v->metadata_fd = open(meta_path, O_RDWR);
    free(meta_path);

    if (v->metadata_fd < 0) {
        perror("open existing metadata");
        goto fail;
    }

    RaidMetaHeader h;
    if (io_full(v->metadata_fd, &h, sizeof(h), 0, 0) < 0)
        goto fail;

    if (memcmp(h.magic, "RAIDVOL1", 8) != 0 ||
        h.version != META_VERSION ||
        h.level != (uint32_t)level ||
        h.num_devices != (uint32_t)num_devices ||
        h.stripe_blocks != stripe_blocks ||
        h.device_blocks != device_blocks ||
        h.virtual_blocks != v->virtual_blocks) {
        fprintf(stderr, "Metadata does not match this volume\n");
        goto fail;
    }

    uint64_t checksum_bytes =
        v->virtual_blocks * sizeof(uint32_t);
    uint64_t valid_offset =
        sizeof(RaidMetaHeader) + checksum_bytes;

    if (io_full(v->metadata_fd, v->checksums,
                checksum_bytes, sizeof(RaidMetaHeader), 0) < 0)
        goto fail;

    if (io_full(v->metadata_fd, v->checksum_valid,
                v->virtual_blocks, valid_offset, 0) < 0)
        goto fail;

    return 0;

fail:
    raidvol_close(v);
    return -1;
}
int raidvol_read_block(RaidVolume *v, uint64_t lba, void *buffer)
{
    if (!v || !buffer || lba >= v->virtual_blocks)
        return -1;

    if (v->level == RAID1) {
        for (int i = 0; i < v->num_devices; i++) {
            if (v->fd[i] >= 0 &&
                read_dev(v, i, lba, buffer) == 0)
               return verify_checksum(v, lba, buffer);
        }
        return -1;
    }

    if (v->level == RAID0) {
        uint64_t stripe = lba / v->stripe_blocks;
        uint64_t offset = lba % v->stripe_blocks;
        int dev = (int)(stripe % (uint64_t)v->num_devices);
        uint64_t physical =
            (stripe / (uint64_t)v->num_devices)
            * v->stripe_blocks + offset;

                if (read_dev(v, dev, physical, buffer) < 0)
            return -1;

        return verify_checksum(v, lba, buffer);
    }

    int data_dev, parity_dev;
    uint64_t physical;

    raid5_map(v, lba, &data_dev, &physical, &parity_dev);

    if (read_dev(v, data_dev, physical, buffer) == 0)
        return verify_checksum(v, lba, buffer);

    /* Reconstruct a missing data block using XOR parity. */
    unsigned char temp[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);

    if (v->fd[parity_dev] < 0 ||
        read_dev(v, parity_dev, physical, buffer) < 0)
        return -1;

    for (int d = 0; d < v->num_devices; d++) {
        if (d == parity_dev || d == data_dev)
            continue;

        if (read_dev(v, d, physical, temp) < 0)
            return -1;

        for (int j = 0; j < BLOCK_SIZE; j++)
            ((unsigned char *)buffer)[j] ^= temp[j];
    }

        return verify_checksum(v, lba, buffer);
}

int raidvol_write_block(RaidVolume *v, uint64_t lba,
                        const void *buffer)
{
    if (!v || !buffer || lba >= v->virtual_blocks)
        return -1;
    if (!v->checksums || !v->checksum_valid)
        return -1;
    if (v->level == RAID1) {
        int success = 0;

        for (int i = 0; i < v->num_devices; i++) {
            if (v->fd[i] >= 0 &&
                write_dev(v, i, lba, buffer) == 0)
                success++;
        }

                if (success == 0)
            return -1;

        v->checksums[lba] = calculate_checksum(buffer);
        v->checksum_valid[lba] = 1;
if (metadata_store_block(v, lba) < 0)
    return -1;
        return 0;
    }

    if (v->level == RAID0) {
        uint64_t stripe = lba / v->stripe_blocks;
        uint64_t offset = lba % v->stripe_blocks;
        int dev = (int)(stripe % (uint64_t)v->num_devices);
        uint64_t physical =
            (stripe / (uint64_t)v->num_devices)
            * v->stripe_blocks + offset;


if (write_dev(v, dev, physical, buffer) < 0)
    return -1;

v->checksums[lba] = calculate_checksum(buffer);
v->checksum_valid[lba] = 1;
if (metadata_store_block(v, lba) < 0)
    return -1;
return 0;    }

    int data_dev, parity_dev;
    uint64_t physical;

    raid5_map(v, lba, &data_dev, &physical, &parity_dev);

    unsigned char old_data[BLOCK_SIZE];
    unsigned char parity[BLOCK_SIZE];
    unsigned char new_parity[BLOCK_SIZE];

    if (v->fd[data_dev] < 0 || v->fd[parity_dev] < 0)
        return -1;

    if (read_dev(v, data_dev, physical, old_data) < 0 ||
        read_dev(v, parity_dev, physical, parity) < 0)
        return -1;

    for (int j = 0; j < BLOCK_SIZE; j++)
        new_parity[j] =
            parity[j] ^ old_data[j] ^ ((const unsigned char *)buffer)[j];

    if (write_dev(v, data_dev, physical, buffer) < 0)
    return -1;

if (write_dev(v, parity_dev, physical, new_parity) < 0)
    return -1;

v->checksums[lba] = calculate_checksum(buffer);
v->checksum_valid[lba] = 1;
if (metadata_store_block(v, lba) < 0)
    return -1;
return 0;
}

int raidvol_rebuild(RaidVolume *v, int failed_device,
                    const char *replacement_path)
{
    if (!v || !replacement_path ||
        failed_device < 0 ||
        failed_device >= v->num_devices ||
        v->level != RAID5)
        return -1;

    int old_fd = v->fd[failed_device];

    int new_fd = open(replacement_path,
                      O_RDWR | O_CREAT | O_TRUNC, 0600);

    if (new_fd < 0) {
        perror("replacement open");
        return -1;
    }

    if (ftruncate(new_fd,
        (off_t)(v->device_blocks * BLOCK_SIZE)) < 0) {
        perror("replacement ftruncate");
        close(new_fd);
        return -1;
    }

    v->fd[failed_device] = -1;
    v->failed_device = failed_device;

    unsigned char result[BLOCK_SIZE];
    unsigned char temp[BLOCK_SIZE];

    for (uint64_t block = 0; block < v->device_blocks; block++) {
        memset(result, 0, BLOCK_SIZE);

        for (int d = 0; d < v->num_devices; d++) {
            if (d == failed_device)
                continue;

            if (read_dev(v, d, block, temp) < 0) {
                v->fd[failed_device] = old_fd;
                close(new_fd);
                return -1;
            }

            for (int j = 0; j < BLOCK_SIZE; j++)
                result[j] ^= temp[j];
        }

        if (io_full(new_fd, result, BLOCK_SIZE,
                    block * BLOCK_SIZE, 1) < 0) {
            v->fd[failed_device] = old_fd;
            close(new_fd);
            return -1;
        }
    }

    if (old_fd >= 0)
        close(old_fd);

    v->fd[failed_device] = new_fd;
    v->failed_device = -1;

    return 0;
}
void raidvol_close(RaidVolume *v)
{
    if (v == NULL) {
        return;
    }

    for (int i = 0; i < RAIDVOL_MAX_DEVICES; i++) {
        if (v->fd[i] >= 0) {
            close(v->fd[i]);
            v->fd[i] = -1;
        }
    }
if (v->metadata_fd >= 0) {
    close(v->metadata_fd);
    v->metadata_fd = -1;
}
    free(v->checksums);
    free(v->checksum_valid);

    v->checksums = NULL;
    v->checksum_valid = NULL;
}
