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
#include <dirent.h>
#include <time.h>
#include <zlib.h>
#include <ftw.h>
#include <sys/types.h>

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

	GITFS_DBG("opendir:: %s", path);
	build_xpath(xpath, path);
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
	void *record;
	struct node *iter_root, *iter;
	struct vfile_record *vfr;
	struct dir_record *dr;
	register int i;
	unsigned char *name;

	dp = (DIR *) (uintptr_t) fi->fh;

	if (!(de = readdir(dp)))
		return -errno;

	/* Fill directories from backing FS */
	do {
		/* Hide the .loose directory, and enumerate only directories */
		if (strcmp(de->d_name, ".loose") && de->d_type == DT_DIR) {
			GITFS_DBG("readdir:: fs: %s", de->d_name);
			if (filler(buf, de->d_name, NULL, 0))
				return -ENOMEM;
		}
	} while ((de = readdir(dp)) != NULL);

	/* Fill files from fstree */
	if (!(dr = find_dr(path)) || !dr->vroot) {
		GITFS_DBG("readdir:: fstree: blank");
		return 0;
	}
	iter_root = dr->vroot;
	iter = dr->vroot;

	/* Use only the leaves */
	while (!iter->is_leaf)
		iter = iter->pointers[0];

	while (1) {
		for (i = 0; i < iter->num_keys; i++) {
			if (!(record = find(iter_root, iter->keys[i], 0)))
				GITFS_DBG("readdir:: key listing issue");
			vfr = (struct vfile_record *) record;
			fill_stat(&st, vfr->history[vfr->HEAD]);
			name = vfr->name + 1; /* Strip leading slash */
			GITFS_DBG("readdir:: tree fill: %s", (const char *) name);
			if (filler(buf, (const char *) name, &st, 0))
				return -ENOMEM;
		}
		if (iter->pointers[BTREE_ORDER - 1] != NULL)
			iter = iter->pointers[BTREE_ORDER - 1];
		else
			break;
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

	GITFS_DBG("access:: %s", path);
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

	GITFS_DBG("rename:: %s to %s", path, newpath);
	build_xpath(xpath, path);
	build_xpath(xnewpath, newpath);
	if (rename(xpath, xnewpath) < 0)
		return -errno;
	return 0;
}

static int gitfs_link(const char *path, const char *newpath)
{
	char xpath[PATH_MAX];
	char xnewpath[PATH_MAX];

	GITFS_DBG("link:: %s to %s", path, newpath);
	build_xpath(xpath, path);
	build_xpath(xnewpath, newpath);
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
	char xpath[PATH_MAX];
	int fd;

	GITFS_DBG("open:: %s", path);
	build_xpath(xpath, path);
	if ((fd = open(xpath, fi->flags)) < 0)
		return -errno;
	fi->fh = fd;

	return 0;
}

static int gitfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	char xpath[PATH_MAX];

	GITFS_DBG("mknod:: %s", path);
	build_xpath(xpath, path);
	if (mknod(xpath, mode, dev) < 0)
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

	GITFS_DBG("create:: %s", path);
	build_xpath(xpath, path);
	if ((fd = creat(xpath, mode)) < 0)
		return -errno;
	fi->fh = fd;

	/* Update the fstree */
	fstree_insert_update_file(path);

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
	FILE *infile, *outfile;
	struct stat st;
	unsigned char sha1[20];
	char outfilename[40];
	char outpath[PATH_MAX];
	char xpath[PATH_MAX];
	int ret;

	GITFS_DBG("release:: %s", path);

	/* Backup this revision before creating a new one */
	build_xpath(xpath, path);
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
	if (zdeflate(infile, outfile, -1) != Z_OK)
		GITFS_DBG("release:: compression problem: %s", outpath);
	fflush(outfile);
	fclose(infile);
	fclose(outfile);
	GITFS_DBG("release:: backup: %s", outpath);

	if (close(fi->fh) < 0)
		return -errno;

	/* Update the fstree */
	fstree_insert_update_file(path);

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

static void preinit(const char *datapath, const char *mountpoint,
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

	GITFS_DBG("preinit:: datapath: %s, mountpoint: %s, fsback: %s",
		rootenv->datapath, rootenv->mountpoint,	rootenv->fsback);
}

static int unlink_cb(const char *path, const struct stat *st,
		int tflag, struct FTW *buf)
{
	if (remove(path) < 0)
		return -errno;
	return 0;
}

/* gitfs mount <packfile> <mountpoint> */
int gitfs_fuse(int argc, char *argv[])
{
	int nargc = 4;
	char **nargv = (char **) malloc(nargc * sizeof(char *));
	char *fsback = "/tmp/gitfs_back";
	char *loosedir = "/tmp/gitfs_back/.loose";
	mode_t dirmode = 0777;
	struct env_t rootenv = {NULL, NULL, NULL, NULL, 0};

	if (!access(fsback, F_OK))
		/* rm -rf fsback */
		if (nftw(fsback, unlink_cb, 64, FTW_DEPTH | FTW_PHYS) < 0)
			die("Unable to remove %s", fsback);
	if (mkdir(fsback, dirmode) < 0)
		return -errno;
	if (mkdir(loosedir, dirmode) < 0)
		return -errno;

	preinit(argv[2], argv[3], fsback, loosedir, &rootenv);
	nargv[0] = argv[0];
	nargv[1] = "-d";
	nargv[2] = "-odefault_permissions";
	nargv[3] = argv[3];
	return fuse_main(nargc, nargv, &gitfs_oper, &rootenv);
}
