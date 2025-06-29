#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../includes/utils.h"
int load_inodes(const char *folder, inode_t *inodes) {
    int index = 0;
    int inodes_per_block = BWFS_BLOCK_SIZE / sizeof(inode_t);
    const long offset_binario = 2000000;

    for (int i = 0; i < INODE_BLOCKS; ++i) {
        char path[256];
        snprintf(path, sizeof(path), "%s/block_%03d.pbm", folder, 1 + i);
        FILE *f = fopen(path, "rb");
        if (!f) continue;

        fseek(f, offset_binario, SEEK_SET);
        int read = fread(&inodes[index], sizeof(inode_t), inodes_per_block, f);
        index += read;
        fclose(f);
    }
    return index;
}
int save_inode(const char *folder, int index, const inode_t *inode) {
    int inodes_per_block = BWFS_BLOCK_SIZE / sizeof(inode_t);
    int block = index / inodes_per_block;
    int offset = index % inodes_per_block;
    const long offset_binario = 2000000;

    char path[256];
    snprintf(path, sizeof(path), "%s/block_%03d.pbm", folder, 1 + block);
    FILE *f = fopen(path, "r+b");
    if (!f) return -1;

    long pos = offset_binario + offset * sizeof(inode_t);
    fseek(f, pos, SEEK_SET);
    fwrite(inode, sizeof(inode_t), 1, f);
    fclose(f);
    return 0;
}

int find_free_inode(const char *folder) {
    uint8_t bitmap[BWFS_INODES] = {0};
    const long offset_binario = 2000000;

    char path[256];
    snprintf(path, sizeof(path), "%s/block_%03d.pbm", folder, 1 + INODE_BLOCKS);
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("❌ No se pudo abrir el archivo de bitmap");
        return -1;
    }

    fseek(f, offset_binario + BWFS_MAX_BLOCKS, SEEK_SET);  // Bitmap de inodos está después del de bloques
    fread(bitmap, sizeof(uint8_t), BWFS_INODES, f);
    fclose(f);

    for (int i = 0; i < BWFS_INODES; ++i)
        if (bitmap[i] == 0)
            return i;

    return -1;
}

int find_free_block(const char *folder) {
    uint8_t block_bitmap[BWFS_MAX_BLOCKS];
    const long offset_binario = 2000000;

    char bpath[256];
    snprintf(bpath, sizeof(bpath), "%s/block_%03d.pbm", folder, 1 + INODE_BLOCKS);
    FILE *f = fopen(bpath, "rb");
    if (!f) {
        perror("❌ Error abriendo archivo de bitmap de bloques");
        return -1;
    }

    fseek(f, offset_binario, SEEK_SET);  // bloque bitmap empieza desde offset_binario
    fread(block_bitmap, sizeof(uint8_t), BWFS_MAX_BLOCKS, f);
    fclose(f);

    for (int i = 6; i < BWFS_MAX_BLOCKS; ++i) {
        if (block_bitmap[i] == 0)
            return i;
    }
    return -1;
}
void update_bitmap_block(const char *folder, int block, int used) {
    uint8_t block_bitmap[BWFS_MAX_BLOCKS];
    const long offset_binario = 2000000;

    char bpath[256];
    snprintf(bpath, sizeof(bpath), "%s/block_%03d.pbm", folder, 1 + INODE_BLOCKS);
    FILE *f = fopen(bpath, "r+b");
    if (!f) {
        perror("❌ Error actualizando bitmap de bloques");
        return;
    }

    fseek(f, offset_binario, SEEK_SET);
    fread(block_bitmap, sizeof(uint8_t), BWFS_MAX_BLOCKS, f);

    block_bitmap[block] = used;

    fseek(f, offset_binario, SEEK_SET);
    fwrite(block_bitmap, sizeof(uint8_t), BWFS_MAX_BLOCKS, f);
    fclose(f);
}

