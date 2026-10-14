
#include "raidvol.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int get_input(const char *prompt, char *buffer, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);

    if (fgets(buffer, (int)size, stdin) == NULL)
        return -1;

    buffer[strcspn(buffer, "\n")] = '\0';

    if (buffer[0] == '\0') {
        printf("Input cannot be empty!\n");
        return -1;
    }

    return 0;
}

static void make_block(unsigned char *block, const char *data)
{
    memset(block, 0, RAIDVOL_BLOCK_SIZE);
    strncpy((char *)block, data, RAIDVOL_BLOCK_SIZE - 1);
}

static int write_and_verify(
    RaidVolume *volume,
    uint64_t lba,
    const unsigned char *data,
    const char *label)
{
    unsigned char readback[RAIDVOL_BLOCK_SIZE];

    if (raidvol_write_block(volume, lba, data) < 0) {
        printf("%s: Write FAILED!\n", label);
        return -1;
    }

    memset(readback, 0, sizeof(readback));

    if (raidvol_read_block(volume, lba, readback) < 0) {
        printf("%s: Read FAILED!\n", label);
        return -1;
    }

    if (memcmp(data, readback, RAIDVOL_BLOCK_SIZE) != 0) {
        printf("%s: Verification FAIL\n", label);
        return -1;
    }

    printf("%s: Write/Read Verification PASS\n", label);
    return 0;
}

int main(void)
{
    int choice;
    int devices;
    RaidLevel level;

    const char *paths[3];

    char data1[RAIDVOL_BLOCK_SIZE];
    char data2[RAIDVOL_BLOCK_SIZE];

    unsigned char block1[RAIDVOL_BLOCK_SIZE];
    unsigned char block2[RAIDVOL_BLOCK_SIZE];
    unsigned char parity[RAIDVOL_BLOCK_SIZE];

    RaidVolume volume;

    printf("\n===== RAIDVol Software Volume Manager =====\n");
    printf("0 - RAID-0 (2 Disks - Striping)\n");
    printf("1 - RAID-1 (2 Disks - Mirroring)\n");
    printf("5 - RAID-5 (3 Disks - Parity)\n");
    printf("Enter RAID level: ");

    if (scanf("%d", &choice) != 1) {
        printf("Invalid input!\n");
        return 1;
    }

    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
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

    printf("\n========== RAID-%d ==========\n", choice);

    if (choice == 0) {
        printf("RAID-0 STRIPING - 2 DISKS\n\n");

        if (get_input("Enter data for Disk 1: ",
                      data1, sizeof(data1)) < 0 ||
            get_input("Enter data for Disk 2: ",
                      data2, sizeof(data2)) < 0)
            return 1;

        /* Requested rule: different inputs must FAIL */
        if (strcmp(data1, data2) != 0) {
            printf("\nDisk 1 input: %s\n", data1);
            printf("Disk 2 input: %s\n", data2);
            printf("\nData mismatch!\n");
            printf("Verification: FAIL\n");
            printf("Disk 1 and Disk 2 data are different.\n");
            return 1;
        }

        printf("\nBoth disk inputs are identical.\n");

    } else if (choice == 1) {
        printf("RAID-1 MIRRORING - 2 DISKS\n\n");

        if (get_input("Enter data for Disk 1: ",
                      data1, sizeof(data1)) < 0 ||
            get_input("Enter data for Disk 2: ",
                      data2, sizeof(data2)) < 0)
            return 1;

        if (strcmp(data1, data2) != 0) {
            printf("\nData mismatch!\n");
            printf("Verification: FAIL\n");
            printf("RAID-1 requires identical input data.\n");
            return 1;
        }

        printf("\nBoth inputs contain the same data.\n");

    } else {
        printf("RAID-5 - 3 DISKS (2 DATA BLOCKS + PARITY)\n\n");

        if (get_input("Enter data block 1: ",
                      data1, sizeof(data1)) < 0 ||
            get_input("Enter data block 2: ",
                      data2, sizeof(data2)) < 0)
            return 1;

        printf("\nRAID-5 calculates parity automatically.\n");
    }

    make_block(block1, data1);
    make_block(block2, data2);

    if (raidvol_create(&volume, level, paths,
                       devices, 64, 4) < 0) {
        printf("Volume creation FAILED!\n");
        return 1;
    }

    printf("\nVolume created successfully!\n");
    printf("Number of disks: %d\n", devices);

    if (choice == 0) {
        if (write_and_verify(&volume, 0, block1, "Disk 1") < 0 ||
            write_and_verify(&volume, 4, block2, "Disk 2") < 0) {
            raidvol_close(&volume);
            return 1;
        }

        printf("\n========== RAID-0 DISK DATA ==========\n");
        printf("Disk 1: %s\n", data1);
        printf("Disk 2: %s\n", data2);
        printf("Verification: PASS\n");

    } else if (choice == 1) {
        if (write_and_verify(&volume, 0, block1, "RAID-1") < 0) {
            raidvol_close(&volume);
            return 1;
        }

        printf("\n========== RAID-1 MIRRORING ==========\n");
        printf("Disk 1: %s\n", data1);
        printf("Disk 2: %s\n", data1);
        printf("Both disks contain the same data.\n");
        printf("Verification: PASS\n");

    } else {
        if (write_and_verify(&volume, 0, block1,
                             "Data block 1") < 0 ||
            write_and_verify(&volume, 4, block2,
                             "Data block 2") < 0) {
            raidvol_close(&volume);
            return 1;
        }

        for (size_t i = 0; i < RAIDVOL_BLOCK_SIZE; i++)
            parity[i] = block1[i] ^ block2[i];

        printf("\n========== RAID-5 DISK LAYOUT ==========\n");
        printf("Disk 1: XOR parity (first 8 bytes): ");

        for (int i = 0; i < 8; i++)
            printf("%02X ", parity[i]);

        printf("\nDisk 2: Data block 1 = %s\n", data1);
        printf("Disk 3: Data block 2 = %s\n", data2);
        printf("Data read-back verification: PASS\n");
    }

    raidvol_close(&volume);
    return 0;
}
