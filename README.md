
# RAIDVol - Software Volume Manager

## Project Overview
RAIDVol is a software volume manager implemented in C on Ubuntu Linux. It demonstrates RAID-0, RAID-1, RAID-5, block read/write operations, and RAID-5 device rebuild using disk image files.

## Objectives
- Implement RAID-0 striping.
- Implement RAID-1 mirroring.
- Implement RAID-5 distributed parity.
- Read and write logical blocks.
- Test RAID-5 device rebuild.

## Technologies Used
- Operating System: Ubuntu Linux
- Programming Language: C
- Compiler: GCC
- Build Tool: GNU Make

## Project Files
- include/raidvol.h - Header file
- src/raidvol.c - RAID implementation
- src/main.c - Basic RAID tests
- tests/rebuild_test.c - Rebuild test
- Makefile - Build and test commands

## How to Compile
Run these commands in the terminal:

    make clean
    make

## How to Run Tests

    make test

## Test Results
- RAID-0: PASS
- RAID-1: PASS
- RAID-5: PASS
- RAID-5 Rebuild: PASS

## Limitations
This is a prototype. Production-grade crash recovery, persistent metadata, checksums, and robust failure handling require further development.

## Conclusion
RAIDVol demonstrates basic RAID storage operations and RAID-5 rebuild verification using C on Linux.
