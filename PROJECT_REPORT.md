# RAIDVol — Software Volume Manager

## Project Report

### 1. Introduction

RAIDVol is a software volume manager implemented in C on Ubuntu Linux. It demonstrates how multiple disk image files can be organized into logical storage volumes using RAID techniques.

### 2. Objectives

* Implement RAID-0 striping.
* Implement RAID-1 mirroring.
* Implement RAID-5 distributed parity.
* Support logical block read and write operations.
* Implement checksum verification.
* Persist metadata across program restarts.
* Support reopening an existing volume.
* Demonstrate RAID-5 device rebuild.
* Measure write performance for different RAID levels.

### 3. Software Requirements

* Operating System: Ubuntu Linux
* Programming Language: C
* Compiler: GCC
* Build Tool: GNU Make

### 4. Implementation

**RAID-0:** Data blocks are distributed across multiple devices using striping.

**RAID-1:** Data is mirrored across devices for redundancy.

**RAID-5:** Data and XOR parity are distributed across multiple devices. Parity helps reconstruct data when a device is unavailable.

**Checksum Verification:** A 32-bit checksum is calculated for written logical blocks and verified when they are read.

**Persistent Metadata:** Metadata is stored in a sidecar file associated with the first device image. The metadata contains the volume configuration, checksums, and block validity information. This allows an existing volume to be reopened without recreating its disk images.

**Reopening Existing Volumes:** The volume-opening function checks the stored configuration and device sizes before restoring metadata.

**Rebuild:** The rebuild test verifies whether logical data remains readable after a replacement device is reconstructed.

### 5. Testing

The following tests have been run successfully:

* RAID-0 block verification: PASS
* RAID-1 block verification: PASS
* RAID-5 block verification: PASS
* RAID-5 rebuild verification: PASS
* Write performance benchmark: Completed
* Metadata persistence test: PASS

### 6. Benchmark Results

Each RAID level was tested by writing 1000 blocks.

| RAID Level | Write Time |
| ---------- | ----------: |
| RAID-0 | 0.008725 seconds |
| RAID-1 | 0.011492 seconds |
| RAID-5 | 0.016878 seconds |

These results are from one benchmark run. Performance may vary depending on system load and storage conditions.

### 7. Limitations

* This is a prototype that uses disk image files.
* The metadata sidecar file must be preserved along with the associated device images.
* Production-grade crash recovery and atomic metadata updates are not guaranteed.
* Concurrent access protection and comprehensive failure handling are not implemented.
* Benchmark results may vary between runs.

### 8. Conclusion

RAIDVol demonstrates RAID-0, RAID-1, and RAID-5 operations, logical block reads and writes, checksum verification, persistent metadata, reopening existing volumes, RAID-5 rebuild testing, and performance benchmarking using C on Linux.
