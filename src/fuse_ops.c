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
    printf("✏️ write: %s (%ld bytes)\n", path, size);

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && strcmp(inodes[i].filename, name) == 0) {
            // Encontramos el archivo
            int block = find_free_block(bwfs_folder);
            if (block < 0) return -ENOSPC;

            // Guardar contenido
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "%s/block_%03d.pbm", bwfs_folder, block);
            FILE *f = fopen(filepath, "r+b");
            if (!f) return -EIO;

            fseek(f, 0, SEEK_SET);
            fwrite(buf, 1, size, f);
            fclose(f);

            // Asociar bloque al inodo
            inodes[i].blocks[0] = block;
            inodes[i].size = size;
            inodes[i].modified_at = time(NULL);
            save_inode(bwfs_folder, i, &inodes[i]);

            // Marcar bloque como usado
            update_bitmap_block(bwfs_folder, block, 1);

            return size;
        }
    }

    return -ENOENT;
}

int bwfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;

    printf("📖 read: %s (offset: %ld, size: %zu)\n", path, offset, size);

    if (!bwfs_folder) {
        fprintf(stderr, "❌ Error: bwfs_folder es NULL en read\n");
        return -EIO;
    }

    const char *name = path + 1;
    inode_t inodes[BWFS_INODES];
    int count = load_inodes(bwfs_folder, inodes);

    for (int i = 0; i < count; ++i) {
        if (inodes[i].used && strcmp(inodes[i].filename, name) == 0) {
            if (offset >= inodes[i].size)
                return 0;  // Nada que leer

            int bytes_to_read = (offset + size > inodes[i].size) ? (inodes[i].size - offset) : size;

            // Usar el primer bloque asignado al archivo
            int block = inodes[i].blocks[0];
            if (block < 0 || block >= BLOCK_COUNT) {
                fprintf(stderr, "❌ Error: bloque inválido en inodo\n");
                return -EIO;
            }

            char fullpath[256];
            snprintf(fullpath, sizeof(fullpath), "%s/block_%03d.pbm", bwfs_folder, block);

            FILE *f = fopen(fullpath, "rb");
            if (!f) {
                perror("❌ fopen falló en read");
                return -EIO;
            }

            fseek(f, offset, SEEK_SET);
            size_t read = fread(buf, 1, bytes_to_read, f);
            fclose(f);

            printf("✅ Se leyeron %zu bytes del archivo %s\n", read, name);
            return read;
        }
    }

    return -ENOENT;
}
