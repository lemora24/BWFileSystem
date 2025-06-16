#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "../includes/bwfs.h"

#define BLOCK_COUNT  128  // cantidad total de bloques
#define INODE_BLOCKS 4    // bloques reservados para inodos
#define BITMAP_BLOCK 1    // bloque reservado para bitmaps

void write_blank_block(const char *path, int block_num) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/block_%03d.pbm", path, block_num);

    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("Error creando bloque");
        exit(1);
    }

    // Imagen PBM en modo texto P1
    fprintf(f, "P1\n");
    fprintf(f, "# Bloque BWFS %d\n", block_num);
    fprintf(f, "32 32\n");  // tamaño simple: 32x32 px
    for (int i = 0; i < 32 * 32; ++i)
        fprintf(f, "0%c", ((i + 1) % 32 == 0) ? '\n' : ' ');

    fclose(f);
}

void write_superblock(const char *path) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/block_000.pbm", path);
    FILE *f = fopen(filename, "ab");
    if (!f) {
        perror("Error abriendo bloque 0");
        exit(1);
    }

    superblock_t sb;
    sb.magic = BWFS_MAGIC;
    sb.total_blocks = BLOCK_COUNT;
    sb.inode_table_start = 1;
    sb.data_block_start = 1 + INODE_BLOCKS + BITMAP_BLOCK;
    sb.free_block_bitmap = 1 + INODE_BLOCKS;
    sb.free_inode_bitmap = 1 + INODE_BLOCKS;

    fwrite(&sb, sizeof(superblock_t), 1, f);
    fclose(f);
}

void write_inode_table(const char *path) {
    inode_t empty_inode;
    memset(&empty_inode, 0, sizeof(inode_t));

    int inodes_per_block = BWFS_BLOCK_SIZE / sizeof(inode_t);
    int total_inodes = BWFS_INODES;
    int written = 0;

    for (int i = 0; i < INODE_BLOCKS; ++i) {
        char filename[256];
        snprintf(filename, sizeof(filename), "%s/block_%03d.pbm", path, 1 + i);
        FILE *f = fopen(filename, "ab");
        if (!f) {
            perror("Error escribiendo inodos");
            exit(1);
        }

        for (int j = 0; j < inodes_per_block && written < total_inodes; ++j) {
            fwrite(&empty_inode, sizeof(inode_t), 1, f);
            written++;
        }

        fclose(f);
    }

    printf("✅ Tabla de inodos inicializada (%d inodos).\n", written);
}

void write_bitmaps(const char *path) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/block_%03d.pbm", path, 1 + INODE_BLOCKS);

    FILE *f = fopen(filename, "r+b");
    if (!f) {
        perror("Error escribiendo bitmaps");
        exit(1);
    }

    uint8_t block_bitmap[BWFS_MAX_BLOCKS] = {0};
    for (int i = 0; i <= 1 + INODE_BLOCKS; ++i)
        block_bitmap[i] = 1;

    uint8_t inode_bitmap[BWFS_INODES] = {0};

    fseek(f, 0, SEEK_END);
    size_t written1 = fwrite(block_bitmap, sizeof(uint8_t), BWFS_MAX_BLOCKS, f);
    size_t written2 = fwrite(inode_bitmap, sizeof(uint8_t), BWFS_INODES, f);

    fclose(f);

    if (written1 != BWFS_MAX_BLOCKS || written2 != BWFS_INODES) {
        fprintf(stderr, "❌ Error: no se escribieron correctamente los bitmaps\n");
        exit(1);
    }

    printf("✅ Bitmaps de bloques e inodos inicializados correctamente.\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: mkfs.bwfs <carpeta_destino>\n");
        return 1;
    }

    const char *folder = argv[1];
    mkdir(folder, 0755);

    printf("🛠️ Creando sistema de archivos BWFS en: %s\n", folder);

    // Crear todos los bloques del sistema
    for (int i = 0; i < BLOCK_COUNT; ++i)
        write_blank_block(folder, i);

    write_superblock(folder);
    write_inode_table(folder);
    write_bitmaps(folder);

    printf("✅ Sistema de archivos creado con %d bloques.\n", BLOCK_COUNT);
    return 0;
}
