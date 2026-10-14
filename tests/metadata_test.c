
#include "raidvol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    RaidVolume v;
    const char *paths[] = {
        "metadata_disk0.img",
        "metadata_disk1.img"
    };

    unsigned char written[RAIDVOL_BLOCK_SIZE];
    unsigned char readback[RAIDVOL_BLOCK_SIZE];

    memset(written, 0x5A, sizeof(written));
    memset(readback, 0, sizeof(readback));

    printf("RAIDVol Metadata Persistence Test\n");

    if (raidvol_create(&v, RAID1, paths, 2, 16, 1) != 0) {
        fprintf(stderr, "Volume creation failed\n");
        return 1;
    }

    if (raidvol_write_block(&v, 3, written) != 0) {
        fprintf(stderr, "Write failed\n");
        raidvol_close(&v);
        return 1;
    }

    raidvol_close(&v);
    printf("Volume closed after writing data.\n");

    if (raidvol_open_existing(&v, RAID1, paths, 2, 16, 1) != 0) {
        fprintf(stderr, "Reopening existing volume failed\n");
        return 1;
    }

    if (raidvol_read_block(&v, 3, readback) != 0) {
        fprintf(stderr, "Read after reopening failed\n");
        raidvol_close(&v);
        return 1;
    }

    if (memcmp(written, readback, sizeof(written)) != 0) {
        fprintf(stderr, "Data mismatch after reopening\n");
        raidvol_close(&v);
        return 1;
    }

    raidvol_close(&v);

    unlink("metadata_disk0.img");
    unlink("metadata_disk1.img");
    unlink("metadata_disk0.img.raidvol.meta");

    printf("Persistence test: PASS\n");
    return 0;
}
