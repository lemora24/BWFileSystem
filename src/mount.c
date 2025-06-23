#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>     // Para PATH_MAX
#include "../includes/fuse_ops.h"
#include <linux/limits.h>

// Estructura de configuración compartida
static struct bwfs_config conf;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: mount.bwfs <carpeta_fs> <punto_de_montaje>\n");
        return 1;
    }

    const char *fs_folder = argv[1];
    const char *mountpoint = argv[2];

    // Obtener ruta absoluta del folder del FS
    static char abs_path[PATH_MAX];
    if (!realpath(fs_folder, abs_path)) {
        perror("❌ Error al obtener ruta absoluta");
        return 1;
    }

    conf.folder = abs_path;

    printf("📁 Ruta absoluta de canvas/: %s\n", conf.folder);

    static struct fuse_operations ops = {
        .init     = bwfs_init,
        .getattr  = bwfs_getattr,
        .readdir  = bwfs_readdir,
        .mkdir    = bwfs_mkdir,
        .create = bwfs_create,
        .utimens = bwfs_utimens,
        .write = bwfs_write,
        .read   = bwfs_read,
        .unlink = bwfs_unlink,
        .rmdir  = bwfs_rmdir,
        .rename = bwfs_rename,
        .opendir = bwfs_opendir,
        .statfs = bwfs_statfs,
        .access = bwfs_access,
        .lseek = bwfs_lseek,
        .open = bwfs_open,

    };

    char *fuse_argv[] = {
        argv[0],
        (char *)mountpoint,
        "-f"  // foreground
    };

    struct fuse_args args = FUSE_ARGS_INIT(3, fuse_argv);
    return fuse_main(args.argc, args.argv, &ops, &conf);
}
