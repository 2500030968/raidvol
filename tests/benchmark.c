#define _POSIX_C_SOURCE 200809L
#include "raidvol.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define TEST_BLOCKS 1000

static double elapsed_seconds(struct timespec start,
                              struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

static int run_benchmark(RaidLevel level,
                         const char **paths,
                         int devices,
                         const char *name)
{
    RaidVolume volume;
    unsigned char buffer[RAIDVOL_BLOCK_SIZE];
    struct timespec start, end;

    if (raidvol_create(&volume, level, paths, devices, 2048, 4) < 0) {
        printf("%s: volume creation FAILED\n", name);
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (uint64_t i = 0; i < TEST_BLOCKS; i++) {
        for (int j = 0; j < RAIDVOL_BLOCK_SIZE; j++) {
            buffer[j] = (unsigned char)((i + j) % 256);
        }

        if (raidvol_write_block(&volume, i, buffer) < 0) {
            printf("%s: write FAILED at block %llu\n",
                   name, (unsigned long long)i);
            raidvol_close(&volume);
            return 1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    printf("%s: wrote %d blocks in %.6f seconds\n",
           name, TEST_BLOCKS, elapsed_seconds(start, end));

    raidvol_close(&volume);
    return 0;
}

int main(void)
{
    const char *raid0[] = {"bench0_disk0.img", "bench0_disk1.img"};
    const char *raid1[] = {"bench1_disk0.img", "bench1_disk1.img"};
    const char *raid5[] = {
        "bench5_disk0.img",
        "bench5_disk1.img",
        "bench5_disk2.img"
    };

    printf("RAIDVol Write Performance Benchmark\n");
    printf("-----------------------------------\n");

    if (run_benchmark(RAID0, raid0, 2, "RAID-0") != 0)
        return 1;

    if (run_benchmark(RAID1, raid1, 2, "RAID-1") != 0)
        return 1;

    if (run_benchmark(RAID5, raid5, 3, "RAID-5") != 0)
        return 1;

    printf("-----------------------------------\n");
    printf("Benchmark completed.\n");

    return 0;
}
