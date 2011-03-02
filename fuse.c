#include "gitfs.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#include <sys/types.h>
#include <sys/xattr.h>

#if 1
#define GITFS_DBG(f, ...) syslog(LOG_DEBUG, f, ## __VA_ARGS__)
#else
#define GITFS_DBG(f, ...) while(0)
#endif

static struct env_t rootenv = {NULL, 0, 0};

void die(const char *err, ...)
{
	va_list params;

	va_start(params, err);
	printf(err, params);
	va_end(params);
}

static void build_xpath(char *xpath, const char *path)
{
	strncpy(xpath, rootenv.path, sizeof(xpath));
	strncat(xpath, path, sizeof(xpath) - strlen(xpath));
}

/**
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * The fuse_context is set up before this function is called, and
 * fuse_get_context()->private_data returns the user_data passed to
 * fuse_main().
 */
void *gitfs_init(struct fuse_conn_info *conn)
{
	return GITFS_DATA;
}

static int gitfs_getattr(const char *path, struct stat *stbuf)
{
	static char xpath[PATH_MAX];

	build_xpath(xpath, path);
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		stbuf->st_uid = rootenv.uid;
		stbuf->st_gid = rootenv.gid;
		stbuf->st_mtime = rootenv.now;
		stbuf->st_atime = rootenv.now;
		stbuf->st_size = 4096;
	} else
		return -ENOENT;
	return 0;
}

static int gitfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;
	static char xpath[PATH_MAX];

	build_xpath(xpath, path);

	/* First fill . and .. */
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	if (!(dp = opendir(xpath)))
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		GITFS_DBG("readdir: path %s", xpath);
		if (filler(buf, de->d_name, NULL, 0))
			return -ENOENT;
	}

	closedir(dp);
	return 0;
}

static int gitfs_open(const char *path, struct fuse_file_info *fi)
{
	static char xpath[PATH_MAX];
	int fd;

	build_xpath(xpath, path);
	GITFS_DBG("open: path %s", xpath);
	fd = open(xpath, fi->flags);
	if (fd < 0)
		return -ENOMEM;
	fi->fh = fd;

	return 0;
}

static int gitfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	GITFS_DBG("open: path %s", xpath);
	if (mknod(path, mode, dev) < 0)
		return -errno;
	return 0;
}

/**
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 */
static int gitfs_create(const char *path, mode_t mode,
			struct fuse_file_info *fi)
{
	char xpath[PATH_MAX];
	int fd;

	build_xpath(xpath, path);
	fd = creat(xpath, mode);
	if (fd < 0)
		return -ENOMEM;
	fi->fh = fd;
	return 0;
}

static int gitfs_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	int retstat = 0;

	retstat = pread(fi->fh, buf, size, offset);
	if (retstat < 0)
		return -ENOMEM;

	return 0;
}

static int gitfs_write(const char *path, const char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	int retstat = 0;

	retstat = pwrite(fi->fh, buf, size, offset);
	if (retstat < 0)
		return -ENOMEM;

	return 0;
}

static int gitfs_readlink(const char *path, char *link, size_t size)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (readlink(path, link, size - 1) < 0)
		return -errno;
	return 0;
}

static int gitfs_mkdir(const char *path, mode_t mode)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (mkdir(xpath, mode) < 0)
		return -errno;
	return 0;
}

static int gitfs_rmdir(const char *path)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (rmdir(xpath) < 0)
		return -ENOENT;
	return 0;
}

static struct fuse_operations gitfs_oper = {
	.init = gitfs_init,
	.getattr = gitfs_getattr,
	.open = gitfs_open,
	.mknod = gitfs_mknod,
	.create = gitfs_create,
	.read = gitfs_read,
	.write = gitfs_write,
	.readdir = gitfs_readdir,
	.readlink = gitfs_readlink,
	.mkdir = gitfs_mkdir,
	.rmdir = gitfs_rmdir,
};

int gitfs_fuse(int argc, char **argv)
{
	struct stat st;

	if (argc != 2)
		die("Usage: %s [mountpoint]", argv[0]);

	/* Set up the rootenv */
	rootenv.path = realpath(argv[1], NULL);
	rootenv.uid = getuid();
	rootenv.gid = getgid();
	rootenv.now = time(NULL);

	GITFS_DBG("gitfs_fuse: rootdir %s, uid %d, gid %d",
		  rootenv.path, rootenv.uid, rootenv.gid);
	if (!rootenv.path)
		die("No mountpoint specified.\n");
	else if (stat(rootenv.path, &st) != 0 || !S_ISDIR(st.st_mode))
		die("error: %s is not a directory.\n", rootenv.path);

	return fuse_main(argc, argv, &gitfs_oper, NULL);
}
