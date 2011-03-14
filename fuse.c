#include "fuse.h"

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

static void build_xpath(char *xpath, const char *path)
{
	strcpy(xpath, ROOTENV->fsback);
	strcat(xpath, path);
}

void *gitfs_init(struct fuse_conn_info *conn)
{
	return ROOTENV;
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
	/* stbuf->st_uid = fuse_get_context()->uid; */
	/* stbuf->st_gid = fuse_get_context()->gid; */
	/* stbuf->st_mtime = ROOTENV->now; */
	/* stbuf->st_atime = ROOTENV->now; */
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
	if (!access(xpath, F_OK)) {
		FILE *infile, *outfile;
		struct stat st;
		unsigned char sha1[20];
		char outfilename[40];
		char outpath[PATH_MAX];
		int ret;

		/* Create a backup in loosedir */
		GITFS_DBG("open: exists: %s", xpath);
		if ((infile = fopen(xpath, "rb")) < 0)
			return -errno;
		if (stat(xpath, &st) < 0)
			return -errno;
		if ((ret = sha1_file(infile, st.st_size, sha1)) < 0)
			return ret;
		print_sha1(outfilename, sha1);
		strcpy(outpath, ROOTENV->loosedir);
		strcat(outpath, "/");
		strcat(outpath, outfilename);
		if ((outfile = fopen(outpath, "wb")) < 0)
			return -errno;
		rewind(infile);
		buffer_copy_bytes(infile, outfile, st.st_size);
		GITFS_DBG("open: backup: %s", outpath);
		fclose(infile);
		fclose(outfile);
	}
	if ((fd = open(xpath, fi->flags)) < 0)
		return -errno;
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
	if ((fd = creat(xpath, mode)) < 0)
		return -errno;
	fi->fh = fd;
	return 0;
}

static int gitfs_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	ssize_t read_bytes;

	if ((read_bytes = pread(fi->fh, buf, size, offset)) < 0)
		return -errno;

	return read_bytes;
}

static int gitfs_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	ssize_t written_bytes;

	if ((written_bytes = pwrite(fi->fh, buf, size, offset)) < 0)
		return -errno;

	return written_bytes;
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
		return -errno;
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
	.symlink = gitfs_symlink,
	.link = gitfs_link,
	.chown = gitfs_chown,
	.chmod = gitfs_chmod,
	.rename = gitfs_rename,
	.truncate = gitfs_truncate,
	.utime = gitfs_utime,
};

void gitfs_subcmd_init(const char *datapath, const char *mountpoint,
		const char *fsback, const char *loosedir,
		struct env_t *rootenv)
{
	struct stat st;

	if (!realpath(fsback, NULL))
		die("Invalid fsback: %s", fsback);
	if (!realpath(loosedir, NULL))
		die("Invalid loosedir: %s", loosedir);
	if (!realpath(datapath, NULL))
		die("Invalid datapath: %s", datapath);
	if (!realpath(mountpoint, NULL))
		die("Invalid mountpoint: %s", mountpoint);
	rootenv->datapath = malloc(sizeof(realpath(datapath, NULL)));
	rootenv->mountpoint = malloc(sizeof(realpath(mountpoint, NULL)));
	rootenv->fsback = malloc(sizeof(realpath(fsback, NULL)));
	rootenv->loosedir = malloc(sizeof(realpath(loosedir, NULL)));
	strcpy(rootenv->fsback, realpath(fsback, NULL));
	strcpy(rootenv->loosedir, realpath(loosedir, NULL));
	strcpy(rootenv->datapath, realpath(datapath, NULL));
	strcpy(rootenv->mountpoint, realpath(mountpoint, NULL));
	rootenv->now = time(NULL);

	/* Check that the datapath refers to a regular file */
	if (stat(rootenv->datapath, &st) != 0 || !S_ISREG(st.st_mode))
		die("%s is not a regular file", datapath);

	GITFS_DBG("datapath: %s, mountpoint: %s, fsback: %s, loosedir: %s",
		rootenv->datapath, rootenv->mountpoint,
		rootenv->fsback, rootenv->loosedir);
}

void gitfs_subcmd_log()
{
}

void gitfs_subcmd_diff()
{
}

int gitfs_fuse(int argc, char *argv[], struct env_t *rootenv)
{
	return fuse_main(argc, argv, &gitfs_oper, rootenv);
}
