#ifndef BWFS_UTILS_H
#define BWFS_UTILS_H

#include "../includes/bwfs.h"
int load_inodes(const char *folder, inode_t *inodes);
int save_inode(const char *folder, int index, const inode_t *inode);
int find_free_inode(const char *folder);
int find_free_block(const char *folder);
void update_bitmap_block(const char *folder, int block, int used);

#endif
