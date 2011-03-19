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

static char xpath[PATH_MAX] = "\0";
static char openpath[PATH_MAX] = "\0";

void *gitfs_init(struct fuse_conn_info *conn)
{
	return ROOTENV;
}

static int gitfs_getattr(const char *path, struct stat *stbuf)
{
	struct file_record *fr;
	int rev;

	rev = parse_pathspec(xpath, path);
	build_xpath(openpath, xpath, rev);
	GITFS_DBG("getattr:: %s %d", openpath, rev);

	/* Get directories and latest files from underlying FS */
	if (!rev) {
		if (lstat(openpath, stbuf) < 0)
			return -errno;
		return 0;
	}

	/* Get history from fstree */
	if (!(fr = find_fr(xpath, rev)))
		return -ENOENT;
	memset(stbuf, 0, sizeof(struct stat));
	fill_stat(stbuf, fr);
	return 0;
}

static int gitfs_fgetattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi)
{
	GITFS_DBG("fgetattr:: %s", path);

	if (fstat(fi->fh, stbuf) < 0)
		return -errno;
	return 0;
}

static int gitfs_opendir(const char *path, struct fuse_file_info *fi)
{
	DIR *dp;

	GITFS_DBG("opendir:: %s", path);
	build_xpath(xpath, path, 0);
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

	dp = (DIR *) (uintptr_t) fi->fh;

	if (!(de = readdir(dp)))
		return -errno;

	/* Fill directories from backing FS */
	do {
		/* Hide the .git directory, and enumerate only directories */
		if (strcmp(de->d_name, ".git") && de->d_type == DT_DIR) {
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
			GITFS_DBG("readdir:: tree fill: %s", (const char *) vfr->name);
			if (filler(buf, (const char *) vfr->name, &st, 0))
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

static int gitfs_access(const char *path, int mask)
{
	GITFS_DBG("access:: %s", path);
	build_xpath(xpath, path, 0);
	if (access(xpath, mask) < 0)
		return -errno;
	return 0;
}

static int gitfs_symlink(const char *path, const char *link)
{
	GITFS_DBG("symlink:: %s to %s", link, path);
	build_xpath(xpath, link, 0);
	if (symlink(path, xpath) < 0)
		return -errno;
	return 0;
}

static int gitfs_rename(const char *path, const char *newpath)
{
	char xnewpath[PATH_MAX];

	GITFS_DBG("rename:: %s to %s", path, newpath);
	build_xpath(xpath, path, 0);
	build_xpath(xnewpath, newpath, 0);
	if (rename(xpath, xnewpath) < 0)
		return -errno;
	return 0;
}

static int gitfs_link(const char *path, const char *newpath)
{
	static char xnewpath[PATH_MAX];

	GITFS_DBG("link:: %s to %s", path, newpath);
	build_xpath(xpath, path, 0);
	build_xpath(xnewpath, newpath, 0);
	if (link(xpath, xnewpath) < 0)
		return -errno;
	return 0;
}

static int gitfs_chmod(const char *path, mode_t mode)
{
	GITFS_DBG("chmod:: %s", path);
	build_xpath(xpath, path, 0);
	if (chmod(xpath, mode) < 0)
		return -errno;
	return 0;
}

static int gitfs_chown(const char *path, uid_t uid, gid_t gid)
{
	/* chown is a no-op */
	return 0;
}

static int gitfs_truncate(const char *path, off_t newsize)
{
	GITFS_DBG("truncate:: %s to %lu", path, newsize);
	build_xpath(xpath, path, 0);
	if (truncate(xpath, newsize) < 0)
		return -errno;
	return 0;
}

static int gitfs_utime(const char *path, struct utimbuf *ubuf)
{
	GITFS_DBG("utime:: %s", path);
	build_xpath(xpath, path, 0);
	if (utime(xpath, ubuf) < 0)
		return -errno;
	return 0;
}

static int gitfs_open(const char *path, struct fuse_file_info *fi)
{
	int flags;
	int rev;
	int fd;

	rev = parse_pathspec(xpath, path);
	if (build_xpath(openpath, xpath, rev) < 0)
		flags = fi->flags;
	else
		flags = (rev ? O_RDONLY : fi->flags);
	GITFS_DBG("open:: %s %d", openpath, rev);
	if ((fd = open(openpath, flags)) < 0)
		return -errno;
	fi->fh = fd;
	return 0;
}

static int gitfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	GITFS_DBG("mknod:: %s", path);
	build_xpath(xpath, path, 0);
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
	int fd;

	/* Always pass through to underlying filesystem */
	GITFS_DBG("create:: %s", path);
	build_xpath(xpath, path, 0);
	if ((fd = creat(xpath, mode)) < 0)
		return -errno;
	fi->fh = fd;

	return 0;
}

static int gitfs_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	ssize_t read_bytes;

	GITFS_DBG("read:: %s", path);
	if ((read_bytes = pread(fi->fh, buf, size, offset)) < 0)
		return -errno;
	return read_bytes;
}

static int gitfs_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	ssize_t written_bytes;

	GITFS_DBG("write:: %s", path);
	if ((written_bytes = pwrite(fi->fh, buf, size, offset)) < 0)
		return -errno;
	return written_bytes;
}

static int gitfs_statfs(const char *path, struct statvfs *statv)
{
	GITFS_DBG("statfs:: %s", path);
	build_xpath(xpath, path, 0);
	if (statvfs(xpath, statv) < 0)
		return -errno;
	return 0;
}

static int gitfs_release(const char *path, struct fuse_file_info *fi)
{
	FILE *infile, *outfile;
	struct stat st;
	unsigned char sha1[20];
	char outfilename[40];
	char outpath[PATH_MAX];
	int rev, ret;

	/* Don't recursively backup history */
	if ((rev = parse_pathspec(xpath, path))) {
		GITFS_DBG("release:: history: %s", path);
		goto END;
	}

	/* Attempt to create a backup */
	build_xpath(xpath, path, 0);
	if ((infile = fopen(xpath, "rb")) < 0 ||
		(stat(xpath, &st) < 0))
		return -errno;
	if ((ret = sha1_file(infile, st.st_size, sha1)) < 0)
		return ret;
	print_sha1(outfilename, sha1);
	sprintf(outpath, "%s/.git/loose/%s", ROOTENV->fsback, outfilename);
	if (!access(outpath, F_OK)) {
		/* SHA1 match; don't overwrite file as an optimization */
		fclose(infile);
		fstree_insert_update_file(path);
		goto END;
	}
	if ((outfile = fopen(outpath, "wb")) < 0) {
		fclose(infile);
		return -errno;
	}
	rewind(infile);
	buffer_copy_bytes(infile, outfile, st.st_size);
	fflush(outfile);
	fclose(infile);
	fclose(outfile);
	GITFS_DBG("release:: backup: %s", outpath);

	if (close(fi->fh) < 0)
		return -errno;

	/* Update the fstree */
	fstree_insert_update_file(path);
	return 0;
END:
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
	build_xpath(xpath, path, 0);
	if (ftruncate(fi->fh, offset) < 0)
		return -errno;
	return 0;
}

static int gitfs_readlink(const char *path, char *link, size_t size)
{
	build_xpath(xpath, path, 0);
	if (readlink(path, link, size - 1) < 0)
		return -errno;
	return 0;
}

static int gitfs_mkdir(const char *path, mode_t mode)
{
	build_xpath(xpath, path, 0);
	if (mkdir(xpath, mode) < 0)
		return -errno;
	return 0;
}

static int gitfs_unlink(const char *path)
{
	/* Always pass through to underlying filesystem */
	GITFS_DBG("unlink:: %s", path);
	fstree_remove_file(path);
	build_xpath(xpath, path, 0);
	if (unlink(xpath) < 0)
		return -errno;
	return 0;
}

static int gitfs_rmdir(const char *path)
{
	/* Always pass through to underlying filesystem */
	GITFS_DBG("rmdir:: %s", path);
	build_xpath(xpath, path, 0);
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
	.fsync = gitfs_fsync,
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

/* gitfs mount <path> <mountpoint> */
/* argv[2] is fsback and argv[3] is the mountpoint */
int gitfs_fuse(int argc, char *argv[])
{
	int nargc;
	char **nargv;
	struct stat st;

	nargc = 4;
	nargv = (char **) malloc(nargc * sizeof(char *));
	struct env_t rootenv;

	/* Sanitize fsback */
	if (!realpath(argv[2], rootenv.fsback))
		die("Invalid fsback: %s", argv[2]);

	if ((lstat(rootenv.fsback, &st) < 0) ||
		(access(rootenv.fsback, R_OK | W_OK | X_OK) < 0))
		die("fsback doesn't have rwx permissions: %s",
			rootenv.fsback);
	if (!S_ISDIR(st.st_mode))
		die("fsback not a directory: %s", rootenv.fsback);

	/* Sanitize mountpoint */
	if (!realpath(argv[3], rootenv.mountpoint))
		die("Invalid mountpoint: %s", argv[3]);

	if ((lstat(rootenv.mountpoint, &st) < 0) ||
		(access(rootenv.mountpoint, R_OK | W_OK | X_OK) < 0))
		die("mountpoint doesn't have rwx permissions: %s",
			rootenv.mountpoint);
	if (!S_ISDIR(st.st_mode))
		die("mountpoint not a directory: %s", rootenv.mountpoint);

	/* Check for .git directory */
	sprintf(xpath, "%s/.git", rootenv.fsback);

	if ((lstat(xpath, &st) < 0) ||
		(access(xpath, R_OK | W_OK | X_OK) < 0))
		die(".git doesn't have rwx permissions: %s", xpath);
	if (!S_ISDIR(st.st_mode))
		die(".git not a directory: %s", xpath);

	/* Check for .git/loose directory */
	sprintf(xpath, "%s/.git/loose", rootenv.fsback);

	if ((lstat(xpath, &st) < 0) ||
		(access(xpath, R_OK | W_OK | X_OK) < 0))
		die(".git/loose doesn't have rwx permissions: %s", xpath);
	if (!S_ISDIR(st.st_mode))
		die(".git/loose not a directory: %s", xpath);

	GITFS_DBG("gitfs_fuse:: fsback: %s, mountpoint: %s",
		rootenv.fsback, rootenv.mountpoint);

	nargv[0] = argv[0];
	nargv[1] = "-d";
	nargv[2] = "-odefault_permissions";
	nargv[3] = argv[3];
	return fuse_main(nargc, nargv, &gitfs_oper, &rootenv);
}
