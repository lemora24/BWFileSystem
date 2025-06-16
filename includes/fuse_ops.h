#ifndef FUSE_OPS_H
#define FUSE_OPS_H

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
struct bwfs_config {
    const char *folder;
};
void *bwfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg);
int bwfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int bwfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int bwfs_mkdir(const char *path, mode_t mode);

#endif
