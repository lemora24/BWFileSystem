#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../includes/bwfs.h"

void read_superblock(const char *path, superblock_t *sb) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/block_000.pbm", path);
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Error abriendo superblock");
        exit(1);
    }

    fseek(f, -sizeof(superblock_t), SEEK_END);  // Se encuentra al final del bloque
    fread(sb, sizeof(superblock_t), 1, f);
    fclose(f);
}

void read_bitmaps(const char *path, uint8_t *block_bitmap, uint8_t *inode_bitmap) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/block_%03d.pbm", path, 1 + INODE_BLOCKS);
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Error leyendo bitmaps");
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    long offset = size - (BWFS_MAX_BLOCKS + BWFS_INODES);
    if (offset < 0) {
        fprintf(stderr, "❌ El archivo %s es demasiado pequeño para contener bitmaps\n", filename);
        fclose(f);
        exit(1);
    }

    fseek(f, offset, SEEK_SET);
    fread(block_bitmap, sizeof(uint8_t), BWFS_MAX_BLOCKS, f);
    fread(inode_bitmap, sizeof(uint8_t), BWFS_INODES, f);
    fclose(f);
}


void print_bitmap(const char *label, const uint8_t *bitmap, int size) {
    printf("🧾 %s:\n", label);
    for (int i = 0; i < size; ++i) {
        if (bitmap[i])
            printf(" [%d]", i);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: fsck.bwfs <carpeta_fs>\n");
        return 1;
    }

    const char *folder = argv[1];
    superblock_t sb;
    uint8_t block_bitmap[BWFS_MAX_BLOCKS] = {0};
    uint8_t inode_bitmap[BWFS_INODES] = {0};

    read_superblock(folder, &sb);

    if (sb.magic != BWFS_MAGIC) {
        printf("❌ Magic inválido. No es un sistema BWFS válido.\n");
        return 1;
    }

    printf("✅ Superblock OK\n");
    printf("  Total de bloques: %u\n", sb.total_blocks);
    printf("  Bloques de datos desde: %u\n", sb.data_block_start);
    printf("  Tabla de inodos desde bloque: %u\n", sb.inode_table_start);

    read_bitmaps(folder, block_bitmap, inode_bitmap);
    print_bitmap("Bloques usados", block_bitmap, BWFS_MAX_BLOCKS);
    print_bitmap("Inodos usados", inode_bitmap, BWFS_INODES);

    printf("✅ fsck finalizado sin errores (fase básica).\n");
    return 0;
}
