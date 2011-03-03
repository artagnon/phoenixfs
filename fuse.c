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
#include <time.h>
#include <sys/types.h>
#include <sys/xattr.h>

#if 1
#define GITFS_DBG(f, ...) \
{ \
	FILE *logfh = fopen("/tmp/gitfs.log", "a");	\
	if (logfh) { \
		fprintf(logfh, "l. %4d: " f "\n", __LINE__, ##__VA_ARGS__); \
		fclose(logfh); \
	} \
}
#else
#define GITFS_DBG(f, ...) while(0)
#endif

static struct env_t rootenv = {NULL, NULL, 0, 0};

static void build_xpath(char *xpath, const char *path)
{
	strcpy(xpath, rootenv.datapath);
	strcat(xpath, path);
}

void *gitfs_init(struct fuse_conn_info *conn)
{
	return fuse_get_context()->private_data;
}

static int gitfs_getattr(const char *path, struct stat *stbuf)
{
	static char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (lstat(xpath, stbuf) < 0)
		return -errno;
	/* memset(stbuf, 0, sizeof(struct stat)); */
	/* stbuf->st_mode = S_IFDIR | 0755; */
	/* stbuf->st_nlink = 2; */
	/* stbuf->st_uid = rootenv.uid; */
	/* stbuf->st_gid = rootenv.gid; */
	/* stbuf->st_mtime = rootenv.now; */
	/* stbuf->st_atime = rootenv.now; */
	/* stbuf->st_size = 4096; */
	return 0;
}

static int gitfs_fgetattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi)
{
	if (fstat(fi->fh, stbuf) < 0)
		return -errno;
	return 0;
}

static int gitfs_opendir(const char *path, struct fuse_file_info *fi)
{
	DIR *dp;
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	GITFS_DBG("opendir: %s", xpath);
	dp = opendir(xpath);
	if (!dp)
		return -errno;
	fi->fh = (intptr_t) dp;
	return 0;
}

static int gitfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;
	struct stat st;

	dp = (DIR *) (uintptr_t) fi->fh;

	if (!readdir(dp))
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		GITFS_DBG("readdir: %s", path);
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, de->d_off))
			return -ENOMEM;
	}
	return 0;
}

static int gitfs_releasedir(const char *path, struct fuse_file_info *fi)
{
	if (closedir((DIR *) (uintptr_t) fi->fh) < 0)
		return -errno;
	return 0;
}

static int gitfs_fsyncdir(const char *path, int datasync,
			struct fuse_file_info *fi)
{
	return 0;
}

static int gitfs_access(const char *path, int mask)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (access(xpath, mask) < 0)
		return -errno;
	return 0;
}

static int gitfs_symlink(const char *path, const char *link)
{
	char xlink[PATH_MAX];

	build_xpath(xlink, link);
	if (symlink(path, xlink) < 0)
		return -errno;
	return 0;
}

static int gitfs_rename(const char *path, const char *newpath)
{
	char xpath[PATH_MAX];
	char xnewpath[PATH_MAX];

	build_xpath(xpath, path);
	build_xpath(xnewpath, newpath);
	GITFS_DBG("rename: %s to %s", xpath, xnewpath);
	if (rename(xpath, xnewpath) < 0)
		return -errno;
	return 0;
}

static int gitfs_link(const char *path, const char *newpath)
{
	char xpath[PATH_MAX];
	char xnewpath[PATH_MAX];

	build_xpath(xpath, path);
	build_xpath(xnewpath, newpath);
	GITFS_DBG("link: %s to %s", xpath, xnewpath);
	if (link(xpath, xnewpath) < 0)
		return -errno;
	return 0;
}

static int gitfs_chmod(const char *path, mode_t mode)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (chmod(xpath, mode) < 0)
		return -errno;
	return 0;
}

static int gitfs_chown(const char *path, uid_t uid, gid_t gid)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (chown(xpath, uid, gid) < 0)
		return -errno;
	return 0;
}

static int gitfs_truncate(const char *path, off_t newsize)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (truncate(xpath, newsize) < 0)
		return -errno;
	return 0;
}

static int gitfs_utime(const char *path, struct utimbuf *ubuf)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (utime(xpath, ubuf) < 0)
		return -errno;
	return 0;
}

static int gitfs_open(const char *path, struct fuse_file_info *fi)
{
	static char xpath[PATH_MAX];
	int fd;

	build_xpath(xpath, path);
	GITFS_DBG("open: %s", xpath);
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
	GITFS_DBG("mknod: %s", xpath);
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

static int gitfs_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	if (pread(fi->fh, buf, size, offset) < 0)
		return -ENOMEM;

	return 0;
}

static int gitfs_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	if (pwrite(fi->fh, buf, size, offset) < 0)
		return -ENOMEM;
	return 0;
}

static int gitfs_statfs(const char *path, struct statvfs *statv)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (statvfs(xpath, statv) < 0)
		return -errno;
	return 0;
}

static int gitfs_flush(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int gitfs_release(const char *path, struct fuse_file_info *fi)
{
	if (close(fi->fh) < 0)
		return -errno;
	return 0;
}

static int gitfs_fsync(const char *path,
		int datasync, struct fuse_file_info *fi)
{
	if (datasync) {
		if (fdatasync(fi->fh) < 0)
			return -errno;
	} else
		if (fsync(fi->fh) < 0)
			return -errno;
	return 0;
}

static int gitfs_ftruncate(const char *path,
			off_t offset, struct fuse_file_info *fi)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (ftruncate(fi->fh, offset) < 0)
		return -errno;
	return 0;
}

static int gitfs_setxattr(const char *path, const char *name,
			const char *value, size_t size, int flags)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (lsetxattr(xpath, name, value, size, flags) < 0)
		return -errno;
	return 0;
}

static int gitfs_getxattr(const char *path, const char *name,
			char *value, size_t size)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (lgetxattr(xpath, name, value, size) < 0)
		return -errno;
	return 0;
}

static int gitfs_listxattr(const char *path, char *list, size_t size)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (llistxattr(xpath, list, size) < 0)
		return -errno;
	return 0;
}

static int gitfs_removexattr(const char *path, const char *name)
{
	char xpath[PATH_MAX];

	build_xpath(xpath, path);
	if (lremovexattr(xpath, name) < 0)
		return -errno;
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

static int gitfs_unlink(const char *path)
{
	char xpath[PATH_MAX];
	build_xpath(xpath, path);
	if (unlink(xpath) < 0)
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
	.fgetattr = gitfs_fgetattr,
	.open = gitfs_open,
	.mknod = gitfs_mknod,
	.releasedir = gitfs_releasedir,
	.create = gitfs_create,
	.read = gitfs_read,
	.write = gitfs_write,
	.statfs = gitfs_statfs,
	.access = gitfs_access,
	.getdir = NULL,
	.readdir = gitfs_readdir,
	.opendir = gitfs_opendir,
	.readlink = gitfs_readlink,
	.mkdir = gitfs_mkdir,
	.rmdir = gitfs_rmdir,
	.unlink = gitfs_unlink,
	.flush = gitfs_flush,
	.fsync = gitfs_fsync,
	.fsyncdir = gitfs_fsyncdir,
	.release = gitfs_release,
	.ftruncate = gitfs_ftruncate,
	.setxattr = gitfs_setxattr,
	.getxattr = gitfs_getxattr,
	.listxattr = gitfs_listxattr,
	.removexattr = gitfs_removexattr,
	.symlink = gitfs_symlink,
	.link = gitfs_link,
	.chown = gitfs_chown,
	.chmod = gitfs_chmod,
	.rename = gitfs_rename,
	.truncate = gitfs_truncate,
	.utime = gitfs_utime,
};

void gitfs_subcmd_init(const char *mountpoint, const char *datapath)
{
	struct stat st;

	/* Set up the rootenv */
	rootenv.datapath = malloc(PATH_MAX * sizeof(char));
	rootenv.mountpoint = malloc(PATH_MAX * sizeof(char));
	strcpy(rootenv.datapath, realpath(datapath, NULL));
	strcpy(rootenv.mountpoint, realpath(mountpoint, NULL));
	rootenv.uid = getuid();
	rootenv.gid = getgid();
	rootenv.now = time(NULL);

	GITFS_DBG("datapath: %s, mountpoint: %s, uid %d, gid %d",
		rootenv.datapath, rootenv.mountpoint, rootenv.uid, rootenv.gid);
	if (!rootenv.mountpoint)
		die("Invalid mountpoint");
	else if (stat(rootenv.mountpoint, &st) != 0 || !S_ISDIR(st.st_mode))
		die("error: %s is not a directory", rootenv.mountpoint);
}

void gitfs_subcmd_log()
{
}

void gitfs_subcmd_diff()
{
}

int gitfs_fuse(int argc, char *argv[])
{
	return fuse_main(argc, argv, &gitfs_oper, NULL);
}
