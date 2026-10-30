#include "simplefs.h"

static void bit_set(unsigned char *bitmap, int index)
{
    bitmap[index / 8] |= (unsigned char)(1u << (index % 8));
}

static long inode_byte_offset(int inode_number)
{
    return (long)INODE_TABLE_BLOCK * BLOCK_SIZE +
           (long)(inode_number - 1) * (long)sizeof(inode_t);
}

static int write_at_block(FILE *fp, int block_number, const void *data, size_t size)
{
    if (fseek(fp, (long)block_number * BLOCK_SIZE, SEEK_SET) != 0) {
        return 0;
    }
    return fwrite(data, size, 1, fp) == 1;
}

static void init_root_inode(inode_t *inode)
{
    memset(inode, 0, sizeof(*inode));
    inode->type      = TYPE_DIRECTORY;
    inode->links     = 2;
    inode->size      = (uint32_t)(2 * sizeof(dirent_t));
    inode->direct[0] = ROOT_DATA_BLOCK;
}

static void init_root_entry(dirent_t *entry, const char *name)
{
    memset(entry, 0, sizeof(*entry));
    entry->inode_no = ROOT_INODE;
    entry->type     = TYPE_DIRECTORY;
    strcpy(entry->name, name);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s --image <image_name>\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "--image") != 0) {
        printf("Error: expected --image option.\n");
        return 1;
    }
    const char *image_name = argv[2];

    FILE *fp = fopen(image_name, "wb+");
    if (!fp) {
        printf("Error: could not create image file.\n");
        return 1;
    }

    unsigned char zero_block[BLOCK_SIZE];
    memset(zero_block, 0, BLOCK_SIZE);
    for (int block = 0; block < TOTAL_BLOCKS; block++) {
        if (fwrite(zero_block, BLOCK_SIZE, 1, fp) != 1) {
            printf("Error: could not initialize image.\n");
            fclose(fp);
            return 1;
        }
    }

    superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic              = MAGIC_NUMBER;
    sb.block_size         = BLOCK_SIZE;
    sb.total_blocks       = TOTAL_BLOCKS;
    sb.inode_count        = TOTAL_INODES;
    sb.inode_bitmap_block = INODE_BITMAP_BLOCK;
    sb.data_bitmap_block  = DATA_BITMAP_BLOCK;
    sb.inode_table_block  = INODE_TABLE_BLOCK;
    sb.data_region_block  = DATA_REGION_BLOCK;
    sb.root_inode         = ROOT_INODE;
    if (!write_at_block(fp, SUPERBLOCK_BLOCK, &sb, sizeof(sb))) {
        printf("Error: could not write superblock.\n");
        fclose(fp);
        return 1;
    }

    unsigned char inode_bitmap[BLOCK_SIZE];
    memset(inode_bitmap, 0, BLOCK_SIZE);
    bit_set(inode_bitmap, ROOT_INODE - 1);
    if (!write_at_block(fp, INODE_BITMAP_BLOCK, inode_bitmap, BLOCK_SIZE)) {
        printf("Error: could not write inode bitmap.\n");
        fclose(fp);
        return 1;
    }

    unsigned char data_bitmap[BLOCK_SIZE];
    memset(data_bitmap, 0, BLOCK_SIZE);
    bit_set(data_bitmap, ROOT_DATA_BLOCK - DATA_REGION_BLOCK);
    if (!write_at_block(fp, DATA_BITMAP_BLOCK, data_bitmap, BLOCK_SIZE)) {
        printf("Error: could not write data bitmap.\n");
        fclose(fp);
        return 1;
    }

    inode_t root_inode;
    init_root_inode(&root_inode);
    fseek(fp, inode_byte_offset(ROOT_INODE), SEEK_SET);
    if (fwrite(&root_inode, sizeof(root_inode), 1, fp) != 1) {
        printf("Error: could not write root inode.\n");
        fclose(fp);
        return 1;
    }

    dirent_t self_entry, parent_entry;
    init_root_entry(&self_entry, ".");
    init_root_entry(&parent_entry, "..");

    fseek(fp, (long)ROOT_DATA_BLOCK * BLOCK_SIZE, SEEK_SET);
    if (fwrite(&self_entry, sizeof(self_entry), 1, fp) != 1 ||
        fwrite(&parent_entry, sizeof(parent_entry), 1, fp) != 1) {
        printf("Error: could not write root directory entries.\n");
        fclose(fp);
        return 1;
    }

    fclose(fp);
    printf("SimpleFS image created successfully: %s\n", image_name);
    return 0;
}
