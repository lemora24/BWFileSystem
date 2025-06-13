#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "../includes/fuse_ops.h"
#include <fcntl.h>
#include <linux/stat.h>
#include "../includes/bwfs.h"
#include "../includes/utils.h"
extern char *bwfs_folder;
void *bwfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void) conn;
    cfg->kernel_cache = 0;
    printf("🎮 BWFS montado correctamente.\n");
    return NULL;
}

int bwfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    return -ENOENT;
}

int bwfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    // Aquí agregarás archivos reales luego

    return 0;
}

int bwfs_mkdir(const char *path, mode_t mode) {
    (void) mode;

    printf("📁 mkdir: %s\n", path);

    if (strcmp(path, "/") == 0)
        return -EEXIST;

    // Extraer nombre sin slash inicial
    const char *name = path + 1;

    int idx = find_free_inode(bwfs_folder);
    if (idx < 0) return -ENOSPC;

    inode_t new_inode = {0};
    new_inode.used = 1;
    new_inode.is_directory = 1;
    strncpy(new_inode.filename, name, BWFS_FILENAME);
    new_inode.size = 0;
    new_inode.created_at = time(NULL);
    new_inode.modified_at = time(NULL);

    save_inode(bwfs_folder, idx, &new_inode);

    // Actualizar bitmap de inodos
    char bpath[256];
    snprintf(bpath, sizeof(bpath), "%s/block_%03d.pbm", bwfs_folder, 1 + INODE_BLOCKS);
    FILE *f = fopen(bpath, "r+b");
    fseek(f, -BWFS_INODES, SEEK_END);
    uint8_t bitmap[BWFS_INODES];
    fread(bitmap, sizeof(uint8_t), BWFS_INODES, f);
    bitmap[idx] = 1;
    fseek(f, -BWFS_INODES, SEEK_END);
    fwrite(bitmap, sizeof(uint8_t), BWFS_INODES, f);
    fclose(f);

    return 0;
}
