
#include "raidvol.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(void)
{
    int choice;
    int devices;
    const char *paths[3];

    RaidLevel level;

    char input[RAIDVOL_BLOCK_SIZE];
    unsigned char written[RAIDVOL_BLOCK_SIZE];
    unsigned char readback[RAIDVOL_BLOCK_SIZE];

    printf("\n===== RAIDVol Software Volume Manager =====\n");
    printf("Select RAID Level:\n");
    printf("0 - RAID-0 (Striping)\n");
    printf("1 - RAID-1 (Mirroring)\n");
    printf("5 - RAID-5 (Parity)\n");
    printf("Enter RAID level: ");

    if (scanf("%d", &choice) != 1) {
        printf("Invalid input!\n");
        return 1;
    }

    if (choice == 0) {
        level = RAID0;
        devices = 2;
        paths[0] = "demo_raid0_disk0.img";
        paths[1] = "demo_raid0_disk1.img";
    } else if (choice == 1) {
        level = RAID1;
        devices = 2;
        paths[0] = "demo_raid1_disk0.img";
        paths[1] = "demo_raid1_disk1.img";
    } else if (choice == 5) {
        level = RAID5;
        devices = 3;
        paths[0] = "demo_raid5_disk0.img";
        paths[1] = "demo_raid5_disk1.img";
        paths[2] = "demo_raid5_disk2.img";
    } else {
        printf("Invalid RAID level!\n");
        return 1;
    }

    printf("Enter data (maximum 511 characters): ");
    if (scanf(" %511[^\n]", input) != 1) {
        printf("Invalid or empty input!\n");
        return 1;
    }

    memset(written, 0, sizeof(written));
    strncpy((char *)written, input, sizeof(written) - 1);

    RaidVolume volume;

    if (raidvol_create(&volume, level, paths, devices, 64, 4) < 0) {
        printf("Volume creation FAILED!\n");
        return 1;
    }

    printf("\nRAID-%d volume created successfully!\n", choice);

    if (raidvol_write_block(&volume, 0, written) < 0) {
        printf("Write FAILED!\n");
        raidvol_close(&volume);
        return 1;
    }

    printf("Data written successfully!\n");

    memset(readback, 0, sizeof(readback));

    if (raidvol_read_block(&volume, 0, readback) < 0) {
        printf("Read FAILED!\n");
        raidvol_close(&volume);
        return 1;
    }

    printf("Data read: %s\n", (char *)readback);

    if (memcmp(written, readback, sizeof(written)) == 0) {
        printf("Verification: PASS\n");
    } else {
        printf("Verification: FAIL\n");
    }

    raidvol_close(&volume);
    return 0;
}
