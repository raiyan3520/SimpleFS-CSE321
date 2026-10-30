# SimpleFS — Simple File System in C

CSE 321 Operating Systems lab project (Summer 2026). Implements an educational disk-image file system with an image builder and a file adder.

## Features

- Creates a 256 KiB image with 64 blocks of 4096 bytes each
- Initializes a superblock, inode bitmap, data bitmap, inode table, and root directory
- Stores up to 31 regular files in the root directory
- Supports at most 3 direct data blocks per file (12 KiB maximum)
- Detects duplicate filenames, oversized files, and some allocation errors

## Requirements

- GCC or another C11 compiler
- A Unix-like environment (Linux, macOS, or WSL); binaries should be compiled on your own machine

## Build

```sh
make
```

Or compile directly:

```sh
gcc -Wall -Wextra -std=c11 simplefs_builder.c -o simplefs_builder
gcc -Wall -Wextra -std=c11 simplefs_adder.c -o simplefs_adder
```

## Run

```sh
./simplefs_builder --image disk.img
./simplefs_adder --input disk.img --file test1.txt
./simplefs_adder --input disk.img --file test2.txt
./simplefs_adder --input disk.img --file test3.txt
```

`disk.img` is a generated file, ignored by Git. The three sample text files are included.

## Disk layout

| Block(s) | Contents |
| --- | --- |
| 0 | Superblock |
| 1 | Inode allocation bitmap |
| 2 | Data-block allocation bitmap |
| 3 | Inode table (32 inodes) |
| 4 | Root directory data |
| 5–63 | Remaining data blocks |

Each inode is 128 bytes and each root directory entry is 64 bytes.

## Limitations

- Root directory only; no nested directories, deletion, renaming, mounting, permissions, or indirect blocks
- No crash recovery or transactional consistency
- Intended for educational use, not real-world storage
- The file adder uses the input argument as the stored filename; filenames containing path separators are not currently handled safely
- Image validation primarily checks the magic number; malformed or truncated images are not fully validated

## Contributors

This project was collaboratively developed as part of the **CSE 321: Operating Systems** course.

### Team Members

1. **Sanchary Roy**
2. **Raiyan Rashed**
3. **Ridika Basak Kotha**

All three members contributed to the development of the SimpleFS project.

## Testing

The builder and adder were compiled with GCC (`-Wall -Wextra -Wpedantic -std=c11`). A basic smoke test confirmed image creation, adding the three sample files, and rejection of a duplicate filename. Edge cases tested: empty file, 4096-byte file, 12288-byte file, and rejection of a 12289-byte file. These tests do not establish complete correctness or security.

