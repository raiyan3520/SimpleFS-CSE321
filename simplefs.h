#ifndef SIMPLEFS_H
#define SIMPLEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define BLOCK_SIZE           4096
#define TOTAL_BLOCKS         64
#define TOTAL_INODES         32

#define SUPERBLOCK_BLOCK     0
#define INODE_BITMAP_BLOCK   1
#define DATA_BITMAP_BLOCK    2
#define INODE_TABLE_BLOCK    3
#define DATA_REGION_BLOCK    4

#define DATA_BLOCKS          (TOTAL_BLOCKS - DATA_REGION_BLOCK)

#define ROOT_INODE           1
#define ROOT_DATA_BLOCK      DATA_REGION_BLOCK

#define MAGIC_NUMBER         0x53465331u

#define MAX_DIRECT_BLOCKS    3
#define MAX_FILE_SIZE        (MAX_DIRECT_BLOCKS * BLOCK_SIZE)

#define TYPE_FILE            1
#define TYPE_DIRECTORY       2

typedef struct {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_count;
    uint32_t inode_bitmap_block;
    uint32_t data_bitmap_block;
    uint32_t inode_table_block;
    uint32_t data_region_block;
    uint32_t root_inode;
} superblock_t;

typedef struct {
    uint16_t type;
    uint16_t links;
    uint32_t size;
    uint32_t direct[MAX_DIRECT_BLOCKS];
    uint8_t  reserved[108];
} inode_t;

typedef struct {
    uint32_t inode_no;
    uint8_t  type;
    char     name[59];
} dirent_t;

#endif /* SIMPLEFS_H */
