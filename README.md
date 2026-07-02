# vsfs-ck: Very Simple File System Consistency Checker

A low-level storage architecture utility written entirely in C that functions as a custom `fsck` (File System Consistency Checker) tool. Engineered for an Ext2-inspired Unix-like disk image configuration (`VSFS`), this tool parses direct binary blocks, evaluates metadata structure health profiles, validates space allocation bitmasks, and reconstructs corrupt storage states on the fly with automatic repair mechanisms.

---

## 🛠️ 1. File System Layout & Memory Geometry

The utility is structured around a static 64-block virtual disk configuration where each block spans exactly 4096 bytes. The binary disk layout maps out into sequential structural zones:

* **Block 0 (Superblock):** Contains fundamental structural constants, file system geometry, configurations, and core signatures.
* **Block 1 (Inode Bitmap):** A single-block bitmask tracking the structural allocation state (free/used) of all system inodes.
* **Block 2 (Data Bitmap):** A single-block bitmask tracking the structural allocation state (free/used) of all raw data blocks.
* **Blocks 3–7 (Inode Table):** A 5-block dedicated table containing a fixed allocation of structures to store individual file metadata metrics (256 bytes per Inode structure, yielding 16 Inodes per block and 80 total Inodes).
* **Blocks 8–63 (Data Blocks):** The remaining 56 blocks dedicated to storing raw user content data and directory structures.

---

## 🚀 2. Core Technical Features & Functional Requirements

### 🔍 Superblock Integrity Validation & Self-Healing
* **Signature Enforcement:** Audits the structural storage gateway cleanly to verify the target constants, matching the system's unique signature magic number entry (`0xd34d`).
* **Geometry Checks:** Cross-checks essential parameters including block sizing variables (`4096` bytes), absolute volume spans (`64` blocks), boundary table block positions, and total index capacities.
* **Auto-Repair Pipeline:** If metadata corruption or bit flips are tracked during diagnostics, the utility dynamically patches fields back to baseline constants and syncs updates to disk block 0 immediately.

### 🔏 Inode Allocation Bitmap Reconstruction
* **Table Scans:** Systematically traverses all 80 allocated entries across the active metadata zone to read current link counters and deletion properties.
* **State Verification:** Validates that active files possess valid links (`links > 0`) and zeroed deletion time counters (`dtime == 0`).
* **Bitmask Correction:** Detects tracking mismatches dynamically. It updates the allocation bitmap, forcing bits active when used blocks are found unregistered, and clearing stale flags when invalid descriptors are mapped.

### 🪵 Hierarchical Multi-Tier Pointer Tracking & Data Repair
* **Block Pointer Scanning:** Recursively traverses block pointer references across multi-tier addressing structures inside active nodes: `direct`, `indirect`, `double_indirect`, and `triple_indirect`.
* **Out-of-Bounds Interception:** Protects system integrity by checking that data pointer locations reside strictly within designated data segment bounds (blocks 8–63).
* **Duplicate Reference Resolution:** Tracks active data block assignments using bit arrays to detect structural sharing collisions.
* **Pointer Isolation & Truncation:** Instantly zeroes out (`0`) bad boundaries or duplicate data references directly inside metadata structures, stopping structural cross-contamination.
* **Indirect Map Traversal:** Automatically reads and validates indirect pointer block tables from disk, scrubbing corrupt data block records back to safe states.

---

## 🏗️ 3. Structural Implementation Highlights

* **Binary Byte Stream Alignment:** Leverages binary file positioning indicators (`fseek`, `fread`, `fwrite`) coupled with low-level structure casting to execute scans without memory padding errors.
* **Bitwise Array Manipulation:** Executes bit-shifts (`1 << bit`) alongside binary operations (`|=`, `&= ~`) to read and overwrite flags directly inside virtual bitmask blocks.
* **State-Driven Disk Synchronizations:** Implements an execution tracking state machine (`any_errors_fixed`), delaying disk writes to block allocations until structural errors are fully evaluated and fixed.

---

## 💻 4. Compilation and Local Execution Guide

### Prerequisites
* GCC or any standard C99-compliant compiler.
* A valid virtual disk image (`vsfs.img`) matching the geometry described above.

### 1. Build the Utility
```bash
gcc -Wall -Wextra -std=c99 vsfs_ck.c -o vsfs-ck
