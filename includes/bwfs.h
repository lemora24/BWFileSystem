#ifndef BWFS_H
#define BWFS_H
#define BWFS_MAGIC      0x42574653  // 'BWFS' en ASCII
#define BWFS_BLOCK_SIZE 1024        // 1 KB por bloque lógico
#define BWFS_MAX_BLOCKS 1024        // Hasta 1024 bloques (ajustable)
#define BWFS_INODES     128         // Cantidad máxima de inodos
#define BWFS_FILENAME   255         // Longitud máxima de nombre
#define BWFS_SIGNATURE  "BWFSv1"    // Firma opcional para el inicio del FS
#define BLOCK_COUNT     128         // Bloques totales del FS
#define INODE_BLOCKS    4           // Bloques reservados para inodos
#define BITMAP_BLOCK    1           // Bloque único para ambos bitmaps
#include <stdint.h>
// Estructura del superbloque (se guarda en el primer bloque)
typedef struct {
    uint32_t magic;              // Identificador único del sistema BWFS
    uint32_t total_blocks;       // Número total de bloques
    uint32_t inode_table_start;  // Posición de inicio de la tabla de inodos
    uint32_t data_block_start;   // Posición de inicio de bloques de datos
    uint32_t free_block_bitmap;  // Posición del bitmap de bloques libres
    uint32_t free_inode_bitmap;  // Posición del bitmap de inodos libres
} superblock_t;

// Estructura de un inodo (archivo o directorio)
typedef struct {
    uint8_t  used;                         // 1 = ocupado, 0 = libre
    uint8_t  is_directory;                // 1 = dir, 0 = archivo
    char     filename[BWFS_FILENAME];     // Nombre del archivo
    uint32_t size;                        // Tamaño del archivo (en bytes)
    uint32_t blocks[12];                  // Bloques directos
    uint32_t created_at;                  // Fecha de creación (timestamp UNIX)
    uint32_t modified_at;                 // Última modificación
} inode_t;

#endif // BWFS_H
