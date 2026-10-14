
#include "raidvol.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(void)
{
    const char *paths[] = {
        "rebuild_disk0.img",
        "rebuild_disk1.img",
        "rebuild_disk2.img"
    };

    const char *replacement = "raid5_rebuilt_disk1.img";

    RaidVolume volume;

    printf("RAID-5 Device Rebuild Test\n");

    if (raidvol_create(
            &volume, RAID5, paths, 3, 64, 4) < 0) {
        printf("Volume creation FAILED\n");
        return 1;
    }

    unsigned char written[RAIDVOL_BLOCK_SIZE];
    unsigned char readback[RAIDVOL_BLOCK_SIZE];

    /* Write known patterns to every virtual block. */
    for (uint64_t block = 0;
         block < volume.virtual_blocks; block++) {

        for (int j = 0; j < RAIDVOL_BLOCK_SIZE; j++) {
            written[j] =
                (unsigned char)((block * 7 + j * 3) % 256);
        }

        if (raidvol_write_block(&volume, block, written) < 0) {
            printf("Write FAILED at block %llu\n",
                   (unsigned long long)block);
            raidvol_close(&volume);
            return 1;
        }
    }

    printf("Initial data written successfully.\n");

    /* Reconstruct device 1 into a replacement image. */
    printf("Rebuilding device 1...\n");

    if (raidvol_rebuild(&volume, 1, replacement) < 0) {
        printf("REBUILD FAILED\n");
        raidvol_close(&volume);
        return 1;
    }

    printf("Rebuild operation completed.\n");

    /* Verify every virtual block after rebuilding. */
    for (uint64_t block = 0;
         block < volume.virtual_blocks; block++) {

        for (int j = 0; j < RAIDVOL_BLOCK_SIZE; j++) {
            written[j] =
                (unsigned char)((block * 7 + j * 3) % 256);
        }

        if (raidvol_read_block(&volume, block, readback) < 0 ||
            memcmp(written, readback,
                   RAIDVOL_BLOCK_SIZE) != 0) {

            printf("VERIFY FAILED at block %llu\n",
                   (unsigned long long)block);
            raidvol_close(&volume);
            return 1;
        }
    }

    printf("Verified blocks: %llu\n",
           (unsigned long long)volume.virtual_blocks);
    printf("Rebuild verification: PASS\n");

    raidvol_close(&volume);

    printf("RAID-5 rebuild test PASSED!\n");
    return 0;
}
