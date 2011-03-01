#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "gitfs.h"

static const char *str = "Hello World!\n";
static const char *path = "/hello";

static int gitfs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else if (strcmp(path, path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(str);
	}
	else
		res = -ENOENT;

	return res;
}

static int gitfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, path + 1, NULL, 0);

	return 0;
}

static int gitfs_open(const char *path, struct fuse_file_info *fi)
{
	if (strcmp(path, path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int gitfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	if (strcmp(path, path) != 0)
		return -ENOENT;

	len = strlen(str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, str + offset, size);
	} else
		size = 0;

	return size;
}

static struct fuse_operations gitfs_oper = {
	.getattr = gitfs_getattr,
	.readdir = gitfs_readdir,
	.open = gitfs_open,
	.read = gitfs_read,
};

int gitfs_fuse(int argc, char **argv)
{
	return fuse_main(argc, argv, &gitfs_oper, NULL);
}
