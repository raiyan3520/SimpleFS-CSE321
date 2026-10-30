#include "simplefs.h"

#define ENTRIES_PER_BLOCK (BLOCK_SIZE / (int)sizeof(dirent_t))

typedef struct {
    unsigned char inode[BLOCK_SIZE];
    unsigned char data[BLOCK_SIZE];
} bitmaps_t;

static void bit_set(unsigned char *bitmap, int index)
{
    bitmap[index / 8] |= (unsigned char)(1u << (index % 8));
}

static int bit_get(const unsigned char *bitmap, int index)
{
    return bitmap[index / 8] & (1u << (index % 8));
}

static long inode_byte_offset(int inode_number)
{
    return (long)INODE_TABLE_BLOCK * BLOCK_SIZE +
           (long)(inode_number - 1) * (long)sizeof(inode_t);
}

/* Scans [start, limit) and returns the first unset bit, or -1 if none free. */
static int first_free_bit(const unsigned char *bitmap, int start, int limit)
{
    for (int i = start; i < limit; i++) {
        if (!bit_get(bitmap, i)) {
            return i;
        }
    }
    return -1;
}

static void load_root_directory(FILE *image, dirent_t entries[])
{
    memset(entries, 0, sizeof(dirent_t) * ENTRIES_PER_BLOCK);
    fseek(image, (long)ROOT_DATA_BLOCK * BLOCK_SIZE, SEEK_SET);
    fread(entries, sizeof(dirent_t), ENTRIES_PER_BLOCK, image);
}

/*
 * Walks the cached root-directory entries once: reports a duplicate name via
 * *is_duplicate, and (regardless) returns the first free slot at index >= 2,
 * or -1 if the directory has no room left.
 */
static int scan_root_directory(const dirent_t entries[], const char *name, int *is_duplicate)
{
    int free_slot = -1;
    *is_duplicate = 0;
    for (int i = 0; i < ENTRIES_PER_BLOCK; i++) {
        if (entries[i].inode_no == 0) {
            if (free_slot == -1 && i >= 2) {
                free_slot = i;
            }
            continue;
        }
        if (strcmp(entries[i].name, name) == 0) {
            *is_duplicate = 1;
        }
    }
    return free_slot;
}

int main(int argc, char *argv[])
{
    if (argc != 5 || strcmp(argv[1], "--input") != 0 || strcmp(argv[3], "--file") != 0) {
        printf("Usage: %s --input <image> --file <file>\n", argv[0]);
        return 1;
    }
    const char *image_name = argv[2];
    const char *file_name  = argv[4];

    if (strlen(file_name) > 58) {
        printf("Error: file name is longer than 58 characters.\n");
        return 1;
    }

    FILE *image = fopen(image_name, "rb+");
    if (!image) {
        printf("Error: file-system image not found.\n");
        return 1;
    }

    superblock_t sb;
    fseek(image, SUPERBLOCK_BLOCK * BLOCK_SIZE, SEEK_SET);
    if (fread(&sb, sizeof(sb), 1, image) != 1 || sb.magic != MAGIC_NUMBER) {
        printf("Error: invalid SimpleFS image.\n");
        fclose(image);
        return 1;
    }

    FILE *source = fopen(file_name, "rb");
    if (!source) {
        printf("Error: source file not found.\n");
        fclose(image);
        return 1;
    }

    fseek(source, 0, SEEK_END);
    long file_size = ftell(source);
    rewind(source);
    if (file_size < 0) {
        printf("Error: could not determine source file size.\n");
        fclose(source);
        fclose(image);
        return 1;
    }
    if (file_size > MAX_FILE_SIZE) {
        printf("Error: file is too large for SimpleFS.\n");
        fclose(source);
        fclose(image);
        return 1;
    }
    int blocks_needed = (int)((file_size + BLOCK_SIZE - 1) / BLOCK_SIZE);

    dirent_t root_entries[ENTRIES_PER_BLOCK];
    load_root_directory(image, root_entries);

    int duplicate_found;
    int free_slot = scan_root_directory(root_entries, file_name, &duplicate_found);
    if (duplicate_found) {
        printf("Error: file already exists in SimpleFS.\n");
        fclose(source);
        fclose(image);
        return 1;
    }
    if (free_slot == -1) {
        printf("Error: root directory is full.\n");
        fclose(source);
        fclose(image);
        return 1;
    }

    bitmaps_t maps;
    fseek(image, INODE_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    fread(maps.inode, BLOCK_SIZE, 1, image);
    fseek(image, DATA_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    fread(maps.data, BLOCK_SIZE, 1, image);

    int inode_bit = first_free_bit(maps.inode, 1, TOTAL_INODES);
    if (inode_bit == -1) {
        printf("Error: no free inode available.\n");
        fclose(source);
        fclose(image);
        return 1;
    }
    int new_inode_number = inode_bit + 1;

    uint32_t allocated[MAX_DIRECT_BLOCKS] = {0};
    for (int i = 0; i < blocks_needed; i++) {
        int data_bit = first_free_bit(maps.data, 0, DATA_BLOCKS);
        if (data_bit == -1) {
            printf("Error: insufficient free data blocks.\n");
            fclose(source);
            fclose(image);
            return 1;
        }
        allocated[i] = (uint32_t)(DATA_REGION_BLOCK + data_bit);
        bit_set(maps.data, data_bit);
    }

    for (int i = 0; i < blocks_needed; i++) {
        unsigned char page[BLOCK_SIZE];
        memset(page, 0, BLOCK_SIZE);
        size_t bytes_read = fread(page, 1, BLOCK_SIZE, source);
        if (bytes_read == 0 && ferror(source)) {
            printf("Error: could not read source file.\n");
            fclose(source);
            fclose(image);
            return 1;
        }
        fseek(image, (long)allocated[i] * BLOCK_SIZE, SEEK_SET);
        if (fwrite(page, BLOCK_SIZE, 1, image) != 1) {
            printf("Error: could not write file data to the image.\n");
            fclose(source);
            fclose(image);
            return 1;
        }
    }

    inode_t file_inode;
    memset(&file_inode, 0, sizeof(file_inode));
    file_inode.type  = TYPE_FILE;
    file_inode.links = 1;
    file_inode.size  = (uint32_t)file_size;
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        file_inode.direct[i] = (i < blocks_needed) ? allocated[i] : 0;
    }
    fseek(image, inode_byte_offset(new_inode_number), SEEK_SET);
    fwrite(&file_inode, sizeof(file_inode), 1, image);

    bit_set(maps.inode, inode_bit);
    fseek(image, INODE_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    fwrite(maps.inode, BLOCK_SIZE, 1, image);
    fseek(image, DATA_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    fwrite(maps.data, BLOCK_SIZE, 1, image);

    dirent_t new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    new_entry.inode_no = (uint32_t)new_inode_number;
    new_entry.type     = TYPE_FILE;
    memcpy(new_entry.name, file_name, strlen(file_name));
    long entry_pos = (long)ROOT_DATA_BLOCK * BLOCK_SIZE +
                      (long)free_slot * (long)sizeof(dirent_t);
    fseek(image, entry_pos, SEEK_SET);
    fwrite(&new_entry, sizeof(new_entry), 1, image);

    inode_t root_inode;
    fseek(image, inode_byte_offset(ROOT_INODE), SEEK_SET);
    fread(&root_inode, sizeof(root_inode), 1, image);
    root_inode.size += (uint32_t)sizeof(dirent_t);
    fseek(image, inode_byte_offset(ROOT_INODE), SEEK_SET);
    fwrite(&root_inode, sizeof(root_inode), 1, image);

    fclose(source);
    fclose(image);
    printf("%s added successfully to %s\n", file_name, image_name);
    return 0;
}
