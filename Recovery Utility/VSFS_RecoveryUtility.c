#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 64
#define INODE_SIZE 256
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define INODE_BLOCKS 5
#define TOTAL_INODES (INODE_BLOCKS * INODES_PER_BLOCK)
#define SUPERBLOCK_BLOCK 0
#define INODE_BITMAP_BLOCK 1
#define DATA_BITMAP_BLOCK 2
#define INODE_TABLE_START 3
#define DATA_BLOCK_START 8
typedef struct{
    uint16_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_bitmap_block;
    uint32_t data_bitmap_block;
    uint32_t inode_table_block;
    uint32_t data_block_start;
    uint32_t inode_size;
    uint32_t inode_count;
    uint8_t reserved[4058];
}Superblock;
typedef struct{
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint32_t links;
    uint32_t blocks;
    uint32_t direct;
    uint32_t indirect;
    uint32_t double_indirect;
    uint32_t triple_indirect;
    uint8_t reserved[156];
} Inode;
FILE *img;
bool any_errors_fixed = false;
void read_block(int block_num, void *buffer){
    fseek(img, block_num * BLOCK_SIZE, SEEK_SET);
    fread(buffer, 1, BLOCK_SIZE, img);
}
void write_block(int block_num, void *buffer){
    fseek(img, block_num * BLOCK_SIZE, SEEK_SET);
    fwrite(buffer, 1, BLOCK_SIZE, img);
    fflush(img);
}
void validate_superblock(Superblock *sb){
    bool dirty = false;
    if (sb->magic != 0xd34d){
        printf("Magic number invalid. Getting it fixed.\n");
        sb->magic = 0xd34d;
        dirty = true;
    }
    if (sb->block_size != BLOCK_SIZE){
        printf("Block size invalid. Getting it fixed.\n");
        sb->block_size = BLOCK_SIZE;
        dirty = true;
    }
    if (sb->total_blocks != TOTAL_BLOCKS){
        printf("Total block count invalid. Getting it fixed.\n");
        sb->total_blocks = TOTAL_BLOCKS;
        dirty = true;
    }
    if (sb->inode_bitmap_block != INODE_BITMAP_BLOCK){
        printf("Inode bitmap block wrong. Getting it fixed.\n");
        sb->inode_bitmap_block = INODE_BITMAP_BLOCK;
        dirty = true;
    }
    if (sb->data_bitmap_block != DATA_BITMAP_BLOCK){
        printf("Data bitmap block wrong. Getting it fixed.\n");
        sb->data_bitmap_block = DATA_BITMAP_BLOCK;
        dirty = true;
    }
    if (sb->inode_table_block != INODE_TABLE_START){
        printf("Inode table start block wrong. Getting it fixed.\n");
        sb->inode_table_block = INODE_TABLE_START;
        dirty = true;
    }
    if (sb->data_block_start != DATA_BLOCK_START){
        printf("Data block start wrong. Getting it fixed.\n");
        sb->data_block_start = DATA_BLOCK_START;
        dirty = true;
    }
    if (sb->inode_size != INODE_SIZE){
        printf("Inode size invalid. Getting it fixed.\n");
        sb->inode_size = INODE_SIZE;
        dirty = true;
    }
    if (sb->inode_count != TOTAL_INODES){
        printf("Inode count invalid. Getting it fixed.\n");
        sb->inode_count = TOTAL_INODES;
        dirty = true;
    }
    if (dirty){
        write_block(SUPERBLOCK_BLOCK, sb);
        any_errors_fixed = true;
    }
}
void fix_inode_bitmap(uint8_t *inode_bitmap, Inode *inodes){
    for (int i = 0; i < TOTAL_INODES; i++){
        bool valid = inodes[i].links > 0 && inodes[i].dtime == 0;
        int byte = i / 8, bit = i % 8;
        if (valid && !(inode_bitmap[byte] & (1 << bit))){
            printf("Inode %d should be marked used. Fixing bitmap.\n", i);
            inode_bitmap[byte] |= (1 << bit);
            any_errors_fixed = true;
        } 
        else if (!valid && (inode_bitmap[byte] & (1 << bit))){
            printf("Inode %d is not valid. Clearing bitmap.\n", i);
            inode_bitmap[byte] &= ~(1 << bit);
            any_errors_fixed = true;
        }
    }
}
bool mark_block(uint32_t blk, int inode_idx, bool *block_used, uint8_t *data_bitmap){
    if (blk < DATA_BLOCK_START || blk >= TOTAL_BLOCKS) {
        printf("Bad data block %u in inode %d. Clearing pointer.\n", blk, inode_idx);
        return false;
    }
    if (block_used[blk]){
        printf("Duplicate block %u found in inode %d. Clearing pointer.\n", blk, inode_idx);
        return false;
    }
    block_used[blk] = true;
    int index = blk - DATA_BLOCK_START;
    int byte = index / 8, bit = index % 8;
    data_bitmap[byte] |= (1 << bit);
    return true;
}
void fix_data_bitmap(uint8_t *data_bitmap, Inode *inodes){
    memset(data_bitmap, 0, BLOCK_SIZE);
    bool block_used[TOTAL_BLOCKS] = {false};
    for (int i = 0; i < TOTAL_INODES; i++){
        if (inodes[i].links == 0 || inodes[i].dtime != 0)
            continue;
        uint32_t ptrs[4] ={
            inodes[i].direct,
            inodes[i].indirect,
            inodes[i].double_indirect,
            inodes[i].triple_indirect
        };
        for (int j = 0; j < 4; j++){
            if (ptrs[j] == 0)
                continue;
            bool valid = mark_block(ptrs[j], i, block_used, data_bitmap);
            if (!valid){
                if (j == 0) {
                    inodes[i].direct = 0;
                }
                else if (j == 1){
                    inodes[i].indirect = 0;
                }
                else if (j == 2){
                    inodes[i].double_indirect = 0;
                }
                else if (j == 3){ 
                    inodes[i].triple_indirect = 0;
                }
                any_errors_fixed = true;
                continue;
            }
            if (j == 1){
                uint32_t indirect_block[BLOCK_SIZE / sizeof(uint32_t)];
                read_block(ptrs[j], indirect_block);
                for (int k = 0; k < BLOCK_SIZE / sizeof(uint32_t); k++){
                    uint32_t blk = indirect_block[k];
                    if (blk == 0)
                        continue;
                    bool valid_indirect = mark_block(blk, i, block_used, data_bitmap);
                    if (!valid_indirect) {
                        printf("Bad or duplicate block %u in indirect block of inode %d. Clearing entry.\n", blk, i);
                        indirect_block[k] = 0;
                        any_errors_fixed = true;
                    }
                }
                write_block(ptrs[j], indirect_block);
            }
        }
    }
}
void read_inodes(Inode *inodes){
    for (int i = 0; i < INODE_BLOCKS; i++){
        fseek(img, (INODE_TABLE_START + i) * BLOCK_SIZE, SEEK_SET);
        fread(((uint8_t*)inodes) + i * BLOCK_SIZE, 1, BLOCK_SIZE, img);
    }
}
void write_inodes(Inode *inodes){
    for (int i = 0; i < INODE_BLOCKS; i++){
        fseek(img, (INODE_TABLE_START + i) * BLOCK_SIZE, SEEK_SET);
        fwrite(((uint8_t*)inodes) + i * BLOCK_SIZE, 1, BLOCK_SIZE, img);
        fflush(img);
    }
}
int main(int argc, char *argv[]){
    if (argc != 2) {
        printf("Usage: %s <vsfs.img>\n", argv[0]);
        return 1;
    }
    img = fopen(argv[1], "r+b");
    if (!img){
        perror("Image open failed");
        return 1;
    }
    Superblock sb;
    read_block(SUPERBLOCK_BLOCK, &sb);
    validate_superblock(&sb);
    uint8_t inode_bitmap[BLOCK_SIZE], data_bitmap[BLOCK_SIZE];
    read_block(INODE_BITMAP_BLOCK, inode_bitmap);
    read_block(DATA_BITMAP_BLOCK, data_bitmap);
    Inode *inodes = malloc(INODE_BLOCKS * BLOCK_SIZE);
    read_inodes(inodes);
    fix_inode_bitmap(inode_bitmap, inodes);
    fix_data_bitmap(data_bitmap, inodes);
    write_block(INODE_BITMAP_BLOCK, inode_bitmap);
    write_block(DATA_BITMAP_BLOCK, data_bitmap);
    write_inodes(inodes);
    if (any_errors_fixed){
        printf("VSFS check complete. Some issues were found and fixed.\n");
    } 
    else{
        printf("VSFS check complete. No issues were detected.\n");
    }
    fclose(img);
    free(inodes);
    return 0;
}
