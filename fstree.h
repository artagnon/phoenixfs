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
	char *datapath;
	char *mountpoint;
	char *fsback;
	char *loosedir;
	time_t now;
};

void build_xpath(char *xpath, const char *path);
void fill_stat(struct stat *st, struct file_record *fr);
void ls_dir(const char *path, struct node *root, struct node *iter);
void insert_update_file(const char *path);
void print_fstree(void);

#define ROOTENV ((struct env_t *) fuse_get_context()->private_data)

#endif
