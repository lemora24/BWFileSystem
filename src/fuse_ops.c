#define FUSE_USE_VERSION 31
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>  
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/stat.h>
#include <fuse3/fuse.h>
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

for (int i = 0; i < count; ++i) {
if (inodes[i].used && inodes[i].is_directory) {
inodes[i].filename[BWFS_FILENAME - 1] = '\0';  // Por seguridad
printf("📂 readdir: inodo %d → nombre: \"%s\"\n", i, inodes[i].filename);
filler(buf, inodes[i].filename, NULL, 0, 0);
}
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
