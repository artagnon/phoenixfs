#ifndef FSTREE_H_
#define FSTREE_H_

#define FUSE_USE_VERSION 26
#define _XOPEN_SOURCE 500
#define GITFS_MAGIC 0x2888

#include "common.h"
#include "btree.h"
#include "crc32.h"
#include "sha1.h"

#include <sys/stat.h>

struct env_t {
	char fsback[PATH_MAX];
	char mountpoint[PATH_MAX];
};

int parse_pathspec(char *xpath, const char *path);
int build_xpath(char *xpath, const char *path, int rev);
void fill_stat(struct stat *st, struct file_record *fr);
struct dir_record *find_dr(const char *path);
struct vfile_record *find_vfr(const char *path);
struct file_record *find_fr(const char *path, int rev);
void fstree_insert_update_file(const char *path);
void fstree_remove_file(const char *path);
void print_fstree(void);

#define ROOTENV ((struct env_t *) fuse_get_context()->private_data)

#endif
