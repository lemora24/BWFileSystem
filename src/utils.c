#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../includes/utils.h"
int load_inodes(const char *folder, inode_t *inodes) {
    int index = 0;
    int inodes_per_block = BWFS_BLOCK_SIZE / sizeof(inode_t);

    for (int i = 0; i < INODE_BLOCKS; ++i) {
        char path[256];
        snprintf(path, sizeof(path), "%s/block_%03d.pbm", folder, 1 + i);
        FILE *f = fopen(path, "rb");
        if (!f) continue;

        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        long inodes_size = inodes_per_block * sizeof(inode_t);
        long bitmap_total = BWFS_MAX_BLOCKS + BWFS_INODES;

        long pos = file_size - bitmap_total - inodes_size;
        fseek(f, pos, SEEK_SET);

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

    char path[256];
    snprintf(path, sizeof(path), "%s/block_%03d.pbm", folder, 1 + block);

    FILE *f = fopen(path, "r+b");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    long inodes_size = inodes_per_block * sizeof(inode_t);
    long bitmap_total = BWFS_MAX_BLOCKS + BWFS_INODES;

    long pos = file_size - bitmap_total - inodes_size + offset * sizeof(inode_t);
    fseek(f, pos, SEEK_SET);

    fwrite(inode, sizeof(inode_t), 1, f);
    fclose(f);
    return 0;
}


int find_free_inode(const char *folder) {
    uint8_t bitmap[BWFS_INODES] = {0};
    char path[256];
    snprintf(path, sizeof(path), "%s/block_%03d.pbm", folder, 1 + INODE_BLOCKS);
    printf("📂 Abriendo archivo de bitmap: %s\n", path);
    snprintf(path, sizeof(path), "%s/block_%03d.pbm", folder, 1 + INODE_BLOCKS);
    printf("📂 Abriendo archivo de bitmap: %s\n", path);

    FILE *test = fopen(path, "r");
    if (test)
    {
        printf("✅ fopen(path, \"r\") funciona.\n");
        fclose(test);
    }
    else
    {
        perror("❌ fopen(path, \"r\") falló");
    }
    printf("📁 folder recibido: %s\n", folder);
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("❌ No se pudo abrir el archivo de bitmap");
        return -1;
    }

    // Posicionarse al final del archivo y leer los últimos bytes (bitmap de inodos)
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < BWFS_INODES)
    {
        fprintf(stderr, "❌ El archivo es demasiado pequeño para contener el bitmap\n");
        fclose(f);
        return -1;
    }
    fseek(f, size - BWFS_INODES, SEEK_SET);
    fread(bitmap, sizeof(uint8_t), BWFS_INODES, f);
    fclose(f);

    // Depuración: mostrar los primeros 10 bits
    printf("🧾 Bitmap leído por find_free_inode: ");
    for (int i = 0; i < 10; ++i)
        printf("%d", bitmap[i]);
    printf("\n");

    for (int i = 0; i < BWFS_INODES; ++i)
        if (bitmap[i] == 0)
            return i;

    return -1;
}
