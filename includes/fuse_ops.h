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
int bwfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int bwfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi);
int bwfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int bwfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int bwfs_unlink(const char *path);
int bwfs_rename(const char *from, const char *to, unsigned int flags);
int bwfs_rmdir(const char *path);
int bwfs_opendir(const char *path, struct fuse_file_info *fi);
int bwfs_statfs(const char *path, struct statvfs *stbuf);
int bwfs_access(const char *path, int mask);
off_t bwfs_lseek(const char *path, off_t offset, int whence, struct fuse_file_info *fi);
int bwfs_open(const char *path, struct fuse_file_info *fi);
int bwfs_flush(const char *path, struct fuse_file_info *fi);
int bwfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);

#endif
