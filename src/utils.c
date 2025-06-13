#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../includes/utils.h"

int load_inodes(const char *folder, inode_t *inodes) {
    int index = 0;
    for (int i = 0; i < INODE_BLOCKS; ++i) {
        char path[256];
        snprintf(path, sizeof(path), "%s/block_%03d.pbm", folder, 1 + i);
        FILE *f = fopen(path, "rb");
        if (!f) continue;

        fseek(f, -BWFS_BLOCK_SIZE, SEEK_END); // leer al final
        int read = fread(&inodes[index], sizeof(inode_t), BWFS_INODES, f);
        index += read;
        fclose(f);
    }
    return index;
}

int save_inode(const char *folder, int index, const inode_t *inode) {
    int block = index / (BWFS_BLOCK_SIZE / sizeof(inode_t));
    int offset = index % (BWFS_BLOCK_SIZE / sizeof(inode_t));

    char path[256];
    snprintf(path, sizeof(path), "%s/block_%03d.pbm", folder, 1 + block);

    FILE *f = fopen(path, "r+b");
    if (!f) return -1;

    fseek(f, BWFS_BLOCK_SIZE - (BWFS_BLOCK_SIZE) + offset * sizeof(inode_t), SEEK_END);
    fwrite(inode, sizeof(inode_t), 1, f);
    fclose(f);
    return 0;
}

int find_free_inode(const char *folder) {
    uint8_t bitmap[BWFS_INODES] = {0};
    char path[256];
    snprintf(path, sizeof(path), "%s/block_%03d.pbm", folder, 1 + INODE_BLOCKS);

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, -BWFS_INODES, SEEK_END);
    fread(bitmap, sizeof(uint8_t), BWFS_INODES, f);
    fclose(f);

    for (int i = 0; i < BWFS_INODES; ++i)
        if (bitmap[i] == 0)
            return i;

    return -1;
}
