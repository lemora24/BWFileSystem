#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../includes/fuse_ops.h"


char *bwfs_folder = NULL;
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: mount.bwfs <carpeta_fs> <punto_de_montaje>\n");
        return 1;
    }

    char *fs_folder = argv[1];
    char *mountpoint = argv[2];

    char *fuse_argv[] = {
        argv[0],
        mountpoint,
        "-f",       // correr en foreground para debug
    };

    struct fuse_args args = FUSE_ARGS_INIT(3, fuse_argv);

    static struct fuse_operations ops = {
        .init     = bwfs_init,
        .getattr  = bwfs_getattr,
        .readdir  = bwfs_readdir,
        .mkdir    = bwfs_mkdir,
    };

    return fuse_main(args.argc, args.argv, &ops, NULL);
}
