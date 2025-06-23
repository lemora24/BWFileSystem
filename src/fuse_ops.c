#define FUSE_USE_VERSION 31
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>  
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/stat.h>
#include <fuse3/fuse.h>
#include <fcntl.h> 
#include "../includes/fuse_ops.h"
#include "../includes/bwfs.h"
#include "../includes/utils.h"


static const char *bwfs_folder = NULL;

void *bwfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void) conn;
    cfg->kernel_cache = 0;

    const struct bwfs_config *conf = fuse_get_context()->private_data;
    bwfs_folder = conf->folder;

    printf("🎮 BWFS montado correctamente.\n");
    return NULL;
}


int bwfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (!bwfs_folder) {
        fprintf(stderr, "❌ Error: bwfs_folder es NULL en getattr\n");
        return -EIO;
    }

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    // Extraer nombre sin slash
    const char *name = path + 1;

    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && strcmp(inodes[i].filename, name) == 0) {
            if (inodes[i].is_directory) {
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
            } else {
                stbuf->st_mode = S_IFREG | 0644;
                stbuf->st_nlink = 1;
                stbuf->st_size = inodes[i].size;
            }

            stbuf->st_ctime = inodes[i].created_at;
            stbuf->st_mtime = inodes[i].modified_at;
            stbuf->st_atime = inodes[i].modified_at;
            return 0;
        }
    }

    return -ENOENT;
}

int bwfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
(void)offset;
(void)fi;
(void)flags;

if (!bwfs_folder) {
fprintf(stderr, "❌ Error: bwfs_folder es NULL en readdir\n");
return -EIO;
}

if (strcmp(path, "/") != 0)
return -ENOENT;

// Entradas obligatorias
filler(buf, ".", NULL, 0, 0);
filler(buf, "..", NULL, 0, 0);

// Cargar los inodos
inode_t inodes[BWFS_INODES];
int count = load_inodes(bwfs_folder, inodes);
printf("📦 Se leyeron %d inodos\n", count);

for (int i = 0; i < BWFS_INODES; ++i) {
    if (!inodes[i].used)
        continue;

    // Asegurar que el nombre esté correctamente terminado
    inodes[i].filename[BWFS_FILENAME - 1] = '\0';

    // Evitar nombres vacíos o corruptos
    if (strlen(inodes[i].filename) == 0)
        continue;

    printf("📂 readdir: inodo %d → nombre: \"%s\"\n", i, inodes[i].filename);
    filler(buf, inodes[i].filename, NULL, 0, 0);
}



return 0;
}

int bwfs_mkdir(const char *path, mode_t mode) {
    (void) mode;

    if (!bwfs_folder) {
        fprintf(stderr, "❌ Error: bwfs_folder es NULL en mkdir\n");
        return -EIO;
    }

    printf("📁 mkdir: %s\n", path);

    if (strcmp(path, "/") == 0)
        return -EEXIST;

    const char *name = path + 1;

    int idx = find_free_inode(bwfs_folder);
    printf("🔍 Resultado de find_free_inode(): %d\n", idx);
    if (idx < 0) return -ENOSPC;

    inode_t new_inode = {0};
    new_inode.used = 1;
    new_inode.is_directory = 1;
    strncpy(new_inode.filename, name, BWFS_FILENAME);
    new_inode.filename[BWFS_FILENAME - 1] = '\0'; // asegurá terminación nula
    printf("📛 Nombre guardado: %s\n", new_inode.filename);
    new_inode.size = 0;
    new_inode.created_at = time(NULL);
    new_inode.modified_at = time(NULL);

    save_inode(bwfs_folder, idx, &new_inode);
    printf("📌 Asignando inodo #%d para %s\n", idx, name);

    char bpath[256];
    snprintf(bpath, sizeof(bpath), "%s/block_%03d.pbm", bwfs_folder, 1 + INODE_BLOCKS);
    FILE *f = fopen(bpath, "r+b");
    if (!f)
        return -EIO;

    fseek(f, -BWFS_INODES, SEEK_END);
    uint8_t bitmap[BWFS_INODES];
    fread(bitmap, sizeof(uint8_t), BWFS_INODES, f);

    printf("🧾 Bitmap antes: ");
    for (int i = 0; i < 10; ++i) printf("%d", bitmap[i]);
    printf("\n");

    bitmap[idx] = 1;

    printf("🧾 Bitmap después: ");
    for (int i = 0; i < 10; ++i) printf("%d", bitmap[i]);
    printf("\n");

    fseek(f, -BWFS_INODES, SEEK_END);
    fwrite(bitmap, sizeof(uint8_t), BWFS_INODES, f);
    fclose(f);

    return 0;
}



int bwfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;  // por ahora no usamos fi
    printf("📝 create: %s\n", path);

    if (strcmp(path, "/") == 0)
        return -EEXIST;

    // Extraer nombre de archivo (sin el slash inicial)
    const char *name = path + 1;

    // Buscar un inodo libre
    int idx = find_free_inode(bwfs_folder);
    printf("🔍 Resultado de find_free_inode(): %d\n", idx);
    if (idx < 0)
        return -ENOSPC;

    // Crear y llenar el nuevo inodo
    inode_t new_inode = {0};
    new_inode.used = 1;
    new_inode.is_directory = 0;  // es archivo, no carpeta
    strncpy(new_inode.filename, name, BWFS_FILENAME);
    new_inode.size = 0;
    new_inode.created_at = time(NULL);
    new_inode.modified_at = time(NULL);

    // Guardar el inodo en disco
    save_inode(bwfs_folder, idx, &new_inode);
    printf("📌 Asignando inodo #%d para archivo %s\n", idx, name);

    // Actualizar bitmap de inodos
    char bpath[256];
    snprintf(bpath, sizeof(bpath), "%s/block_%03d.pbm", bwfs_folder, 1 + INODE_BLOCKS);
    FILE *f = fopen(bpath, "r+b");
    if (!f)
        return -EIO;

    fseek(f, -BWFS_INODES, SEEK_END);
    uint8_t bitmap[BWFS_INODES];
    fread(bitmap, sizeof(uint8_t), BWFS_INODES, f);
    bitmap[idx] = 1;
    fseek(f, -BWFS_INODES, SEEK_END);
    fwrite(bitmap, sizeof(uint8_t), BWFS_INODES, f);
    fclose(f);

    return 0;
}
int bwfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void)fi;
    if (!bwfs_folder) {
        fprintf(stderr, "❌ Error: bwfs_folder es NULL en utimens\n");
        return -EIO;
    }

    if (strcmp(path, "/") == 0)
        return 0;  // OK para la raíz

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && strcmp(inodes[i].filename, name) == 0) {
            inodes[i].modified_at = tv[1].tv_sec;
            inodes[i].created_at = tv[0].tv_sec;
            save_inode(bwfs_folder, i, &inodes[i]);
            printf("⏱️ utimens aplicado a %s\n", name);
            return 0;
        }
    }

    return -ENOENT;
}

int bwfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;
    printf("✏️ write: %s (offset: %ld, size: %ld)\n", path, offset, size);

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && strcmp(inodes[i].filename, name) == 0) {
            int block = inodes[i].blocks[0];

            // Si no hay bloque asignado aún, pedimos uno
            if (block < 0 || block >= BLOCK_COUNT) {
                block = find_free_block(bwfs_folder);
                if (block < 0) return -ENOSPC;
                inodes[i].blocks[0] = block;
            }

            char filepath[256];
            snprintf(filepath, sizeof(filepath), "%s/block_%03d.pbm", bwfs_folder, block);

            // Leer encabezado y datos existentes
            FILE *f = fopen(filepath, "r");
            if (!f) return -EIO;

            // Saltar encabezado
            char line1[64], line2[64], line3[64];
            fgets(line1, sizeof(line1), f);
            fgets(line2, sizeof(line2), f);
            fgets(line3, sizeof(line3), f);

            // Leer todo el bitmap actual (hasta 1000000 bits = 125000 bytes)
            unsigned char current_data[125000] = {0};
            int bit_index = 0;
            char ch;
            while (fscanf(f, " %c", &ch) == 1 && bit_index < 1000000) {
                if (ch != '0' && ch != '1') continue;
                int byte_idx = bit_index / 8;
                current_data[byte_idx] = (current_data[byte_idx] << 1) | (ch - '0');
                bit_index++;
            }
            fclose(f);

            // Insertar nuevos datos respetando offset
            if (offset + size > sizeof(current_data))
                size = sizeof(current_data) - offset;  // evitar overflow

            memcpy(current_data + offset, buf, size);

            // Reescribir el archivo desde cero
            f = fopen(filepath, "w");
            if (!f) return -EIO;

            fprintf(f, "P1\n# Bloque BWFS\n1000 1000\n");
            int written_bits = 0;
            for (int i = 0; i < sizeof(current_data) && written_bits < 1000000; ++i) {
                for (int b = 7; b >= 0 && written_bits < 1000000; --b) {
                    int bit = (current_data[i] >> b) & 1;
                    fprintf(f, "%d ", bit);
                    written_bits++;
                    if (written_bits % 1000 == 0) fprintf(f, "\n");
                }
            }

            fclose(f);

            inodes[i].size = (offset + size > inodes[i].size) ? (offset + size) : inodes[i].size;
            inodes[i].modified_at = time(NULL);
            save_inode(bwfs_folder, i, &inodes[i]);

            update_bitmap_block(bwfs_folder, block, 1);

            return size;
        }
    }

    return -ENOENT;
}

int bwfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;

    printf("📖 read: %s (offset: %ld, size: %zu)\n", path, offset, size);

    if (!bwfs_folder)
        return -EIO;

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && strcmp(inodes[i].filename, name) == 0) {
            if (offset >= inodes[i].size)
                return 0;

            int bytes_to_read = (offset + size > inodes[i].size) ? (inodes[i].size - offset) : size;
            int block = inodes[i].blocks[0];

            if (block < 0 || block >= BLOCK_COUNT)
                return -EIO;

            char filepath[256];
            snprintf(filepath, sizeof(filepath), "%s/block_%03d.pbm", bwfs_folder, block);

            FILE *f = fopen(filepath, "r");
            if (!f)
                return -EIO;

            // Saltar encabezado (3 líneas)
            char header[256];
            fgets(header, sizeof(header), f);  // P1
            fgets(header, sizeof(header), f);  // # comentario
            fgets(header, sizeof(header), f);  // dimensiones

            int bit_index = 0, byte_index = 0;
            unsigned char byte = 0;
            char bitchar;
            while ((fscanf(f, " %c", &bitchar) == 1) && byte_index < (int)(offset + bytes_to_read)) {
                if (bitchar != '0' && bitchar != '1')
                    continue;

                byte = (byte << 1) | (bitchar - '0');
                bit_index++;

                if (bit_index == 8) {
                    if (byte_index >= (int)offset)
                        buf[byte_index - offset] = byte;
                    byte_index++;
                    bit_index = 0;
                    byte = 0;
                }
            }

            if (bit_index > 0 && byte_index >= (int)offset && byte_index < (int)(offset + bytes_to_read)) {
                byte <<= (8 - bit_index);
                buf[byte_index - offset] = byte;
                byte_index++;
            }

            fclose(f);
            printf("✅ Se leyeron %d bytes\n", byte_index - offset);
            return byte_index - offset;
        }
    }

    return -ENOENT;
}

int bwfs_unlink(const char *path) {
    printf("❌ unlink: %s\n", path);

    if (!bwfs_folder) {
        fprintf(stderr, "❌ Error: bwfs_folder es NULL en unlink\n");
        return -EIO;
    }

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && !inodes[i].is_directory && strcmp(inodes[i].filename, name) == 0) {
            int block = inodes[i].blocks[0];

            // iberar bloque si existe
            if (block >= 0 && block < BLOCK_COUNT) {
                update_bitmap_block(bwfs_folder, block, 0);
                printf("🧹 Bloque %d liberado\n", block);
            }

            // Limpiar el inodo
            memset(&inodes[i], 0, sizeof(inode_t));
            save_inode(bwfs_folder, i, &inodes[i]);
            printf("🗑️ Inodo %d limpiado\n", i);

            // Actualizar bitmap de inodos
            char bpath[256];
            snprintf(bpath, sizeof(bpath), "%s/block_%03d.pbm", bwfs_folder, 1 + INODE_BLOCKS);
            FILE *f = fopen(bpath, "r+b");
            if (!f)
                return -EIO;

            fseek(f, -BWFS_INODES, SEEK_END);
            uint8_t bitmap[BWFS_INODES];
            fread(bitmap, sizeof(uint8_t), BWFS_INODES, f);

            bitmap[i] = 0;

            fseek(f, -BWFS_INODES, SEEK_END);
            fwrite(bitmap, sizeof(uint8_t), BWFS_INODES, f);
            fclose(f);

            printf("✅ Archivo '%s' eliminado correctamente\n", name);
            return 0;
        }
    }

    return -ENOENT;
}
int bwfs_rmdir(const char *path) {
    printf("🧺 rmdir: %s\n", path);

    if (!bwfs_folder) {
        fprintf(stderr, "❌ Error: bwfs_folder es NULL en rmdir\n");
        return -EIO;
    }

    if (strcmp(path, "/") == 0)
        return -EBUSY;  // no se puede eliminar la raíz

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);
    int target = -1;

    // Buscar el directorio por nombre
    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && inodes[i].is_directory &&
            strcmp(inodes[i].filename, name) == 0) {
            target = i;
            break;
        }
    }

    if (target == -1)
        return -ENOENT;

    // Verificar que esté vacío (sin archivos o subdirectorios asociados)
    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && i != target &&
            strcmp(inodes[i].filename, name) == 0) {
            return -ENOTEMPTY;
        }
    }

    // Borrar el inodo
    memset(&inodes[target], 0, sizeof(inode_t));
    save_inode(bwfs_folder, target, &inodes[target]);
    printf("🧽 Inodo %d del directorio '%s' eliminado\n", target, name);

    // Actualizar bitmap de inodos
    char bpath[256];
    snprintf(bpath, sizeof(bpath), "%s/block_%03d.pbm", bwfs_folder, 1 + INODE_BLOCKS);
    FILE *f = fopen(bpath, "r+b");
    if (!f)
        return -EIO;

    fseek(f, -BWFS_INODES, SEEK_END);
    uint8_t bitmap[BWFS_INODES];
    fread(bitmap, sizeof(uint8_t), BWFS_INODES, f);

    bitmap[target] = 0;

    fseek(f, -BWFS_INODES, SEEK_END);
    fwrite(bitmap, sizeof(uint8_t), BWFS_INODES, f);
    fclose(f);

    printf("✅ Carpeta '%s' eliminada correctamente\n", name);
    return 0;
}
int bwfs_rename(const char *from, const char *to, unsigned int flags) {
    (void)flags;
    printf("✏️ rename: %s → %s\n", from, to);

    if (!bwfs_folder)
        return -EIO;

    if (strcmp(from, "/") == 0)
        return -EBUSY;

    const char *name_from = from + 1;
    const char *name_to = to + 1;

    if (strlen(name_to) == 0 || strlen(name_to) >= BWFS_FILENAME)
        return -EINVAL;

    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && strcmp(inodes[i].filename, name_from) == 0) {

            // Verificar que no exista otro archivo con el nombre nuevo
            for (int j = 0; j < count; ++j) {
                if (inodes[j].used && strcmp(inodes[j].filename, name_to) == 0)
                    return -EEXIST;
            }

            // Renombrar
            strncpy(inodes[i].filename, name_to, BWFS_FILENAME);
            inodes[i].filename[BWFS_FILENAME - 1] = '\0';
            inodes[i].modified_at = time(NULL);
            save_inode(bwfs_folder, i, &inodes[i]);

            printf("✅ Renombrado inodo %d: %s → %s\n", i, name_from, name_to);
            return 0;
        }
    }

    return -ENOENT;
}
int bwfs_opendir(const char *path, struct fuse_file_info *fi) {
    printf("📂 opendir: %s\n", path);

    if (!bwfs_folder)
        return -EIO;

    if (strcmp(path, "/") == 0)
        return 0;  // raíz siempre válida

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used &&
            inodes[i].is_directory &&
            strcmp(inodes[i].filename, name) == 0) {
            return 0;  // Directorio válido
        }
    }

    return -ENOENT;  // No encontrado
}
int bwfs_statfs(const char *path, struct statvfs *stbuf) {
    (void)path;  // no lo usamos directamente
    printf("📊 statfs solicitado\n");

    if (!bwfs_folder)
        return -EIO;

    memset(stbuf, 0, sizeof(struct statvfs));

    // Cargar el superbloque
    char superpath[256];
    snprintf(superpath, sizeof(superpath), "%s/block_000.pbm", bwfs_folder);
    FILE *f = fopen(superpath, "rb");
    if (!f)
        return -EIO;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, size - sizeof(superblock_t), SEEK_SET);
    superblock_t sb;
    fread(&sb, sizeof(superblock_t), 1, f);
    fclose(f);

    // Asumimos bloques de 125000 bytes útiles (1000x1000 bits)
    stbuf->f_bsize = 125000;             // Tamaño de bloque
    stbuf->f_frsize = 125000;            // Tamaño de fragmento
    stbuf->f_blocks = sb.total_blocks;   // Total de bloques
    stbuf->f_bfree = 0;                  // Lo calculamos ahora

    // Cargar bitmap para contar bloques libres
    char bmpath[256];
    snprintf(bmpath, sizeof(bmpath), "%s/block_%03d.pbm", bwfs_folder, 1 + INODE_BLOCKS);
    f = fopen(bmpath, "rb");
    if (!f)
        return -EIO;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    if (size < BWFS_MAX_BLOCKS + BWFS_INODES) {
        fclose(f);
        return -EIO;
    }

    fseek(f, size - (BWFS_MAX_BLOCKS + BWFS_INODES), SEEK_SET);
    uint8_t block_bitmap[BWFS_MAX_BLOCKS];
    fread(block_bitmap, sizeof(uint8_t), BWFS_MAX_BLOCKS, f);
    fclose(f);

    int free_blocks = 0;
    for (int i = 0; i < sb.total_blocks; ++i) {
        if (block_bitmap[i] == 0)
            free_blocks++;
    }

    stbuf->f_bfree = free_blocks;
    stbuf->f_bavail = free_blocks;

    // Inodos
    stbuf->f_files = BWFS_INODES;
    stbuf->f_ffree = 0;

    for (int i = 0; i < BWFS_INODES; ++i) {
        if (i >= sizeof(block_bitmap)) break;  // seguridad
        if (block_bitmap[i] == 0)
            stbuf->f_ffree++;
    }

    return 0;
}

int bwfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
    (void)isdatasync;
    (void)fi;
    printf("🔃 fsync: %s\n", path);

    if (!bwfs_folder)
        return -EIO;

    return 0;
}
    // Como no usamos buffering, no hay nada que hacer realmente.

int bwfs_flush(const char *path, struct fuse_file_info *fi) {
    (void)fi;
    printf("🧹 flush: %s\n", path);

    return 0;
}


int bwfs_access(const char *path, int mask) {
    printf("🔐 access: %s (mask: %d)\n", path, mask);

    if (!bwfs_folder)
        return -EIO;

    if (strcmp(path, "/") == 0)
        return 0;  // raíz siempre accesible

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && strcmp(inodes[i].filename, name) == 0) {
            return 0;
        }
    }

    return -ENOENT;
}

off_t bwfs_lseek(const char *path, off_t offset, int whence, struct fuse_file_info *fi) {
    (void)fi;
    printf("📍 lseek: %s (offset: %ld, whence: %d)\n", path, offset, whence);

    if (!bwfs_folder)
        return -EIO;

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && strcmp(inodes[i].filename, name) == 0) {
            off_t result = 0;

            switch (whence) {
                case SEEK_SET:
                    result = offset;
                    break;
                case SEEK_CUR:
                    result = fi->fh + offset;
                    break;
                case SEEK_END:
                    result = inodes[i].size + offset;
                    break;
                default:
                    return -EINVAL;
            }

            if (result < 0)
                return -EINVAL;

            return result;
        }
    }

    return -ENOENT;
}
int bwfs_open(const char *path, struct fuse_file_info *fi) {
    printf("📂 open: %s\n", path);

    if (!bwfs_folder)
        return -EIO;

    if (strcmp(path, "/") == 0)
        return -EISDIR;

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used &&
            strcmp(inodes[i].filename, name) == 0 &&
            !inodes[i].is_directory) {
            // Podés guardar info en fi->fh si lo necesitás luego
            return 0;
        }
    }

    return -ENOENT;
}
