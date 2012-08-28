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

void *phoenixfs_init(struct fuse_conn_info *conn)
{
	return ROOTENV;
}

static int phoenixfs_getattr(const char *path, struct stat *stbuf)
{
	struct file_record *fr;
	int rev;

	rev = parse_pathspec(xpath, path);
	build_xpath(openpath, xpath, rev);
	PHOENIXFS_DBG("getattr:: %s %d", openpath, rev);

	/* Try underlying FS */
	if (lstat(openpath, stbuf) < 0) {
		/* Try fstree */
		if (!(fr = find_fr(xpath, rev)))
			return -ENOENT;
	} else
		return 0;

	memset(stbuf, 0, sizeof(struct stat));
	fill_stat(stbuf, fr);
	return 0;
}

static int phoenixfs_fgetattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi)
{
	PHOENIXFS_DBG("fgetattr:: %s", path);

	if (fstat(fi->fh, stbuf) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_opendir(const char *path, struct fuse_file_info *fi)
{
	DIR *dp;

	PHOENIXFS_DBG("opendir:: %s", path);
	build_xpath(xpath, path, 0);
	dp = opendir(xpath);
	if (!dp)
		return -errno;
	fi->fh = (intptr_t) dp;
	return 0;
}

static int phoenixfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
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
			PHOENIXFS_DBG("readdir:: fs: %s", de->d_name);
			if (filler(buf, de->d_name, NULL, 0))
				return -ENOMEM;
		}
	} while ((de = readdir(dp)) != NULL);

	/* Fill files from fstree */
	if (!(dr = find_dr(path)) || !dr->vroot) {
		PHOENIXFS_DBG("readdir:: fstree: blank");
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
				PHOENIXFS_DBG("readdir:: key listing issue");
			vfr = (struct vfile_record *) record;
			fill_stat(&st, vfr->history[vfr->HEAD]);
			PHOENIXFS_DBG("readdir:: tree fill: %s", (const char *) vfr->name);
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

static int phoenixfs_releasedir(const char *path, struct fuse_file_info *fi)
{
	if (closedir((DIR *) (uintptr_t) fi->fh) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_access(const char *path, int mask)
{
	PHOENIXFS_DBG("access:: %s", path);
	build_xpath(xpath, path, 0);
	if (access(xpath, mask) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_symlink(const char *path, const char *link)
{
	char xlink[PATH_MAX];

	PHOENIXFS_DBG("symlink:: %s to %s", link, path);
	sprintf(xpath, "%s/%s", ROOTENV->fsback, path);
	build_xpath(xlink, link, 0);
	if (symlink(xpath, xlink) < 0)
		return -errno;
	fstree_insert_update_file(link, path);
	return 0;
}

static int phoenixfs_rename(const char *path, const char *newpath)
{
	char xnewpath[PATH_MAX];
	struct dir_record *dr;
	struct vfile_record *vfr, *new_vfr;
	char *filename, *newfilename;
	uint16_t key = ~0;
	size_t length;
	uint8_t start_rev, rev_nr;

	PHOENIXFS_DBG("rename:: %s to %s", path, newpath);
	build_xpath(xpath, path, 0);
	build_xpath(xnewpath, newpath, 0);
	if (rename(xpath, xnewpath) < 0)
		return -errno;

	/* Update fstree */
	filename = split_basename(path, xpath);
	if (!(dr = find_dr(xpath))) {
		PHOENIXFS_DBG("rename:: Missing dr for %s", xpath);
		return 0;
	}

	/* Find the old vfr to copy out data from and remove */
	length = (size_t) strlen((char *) filename);
	key = compute_crc32(key, (const unsigned char *) filename, length);
	if (!(vfr = find(dr->vroot, key, 0))) {
		PHOENIXFS_DBG("rename:: Missing vfr for %s", path);
		return 0;
	}

	/* Make a new vfr and copy out history from old vfr */
	newfilename = split_basename(path, NULL);
	new_vfr = make_vfr(newfilename);

	/* Compute start_rev and rev_nr */
	if (vfr->HEAD < 0) {
		start_rev = 0;
		rev_nr = 0;
	} else if (vfr->history[(vfr->HEAD + 1) % REV_TRUNCATE]) {
		/* History is full, and is probably wrapping around */
		start_rev = (vfr->HEAD + 1) % REV_TRUNCATE;
		rev_nr = 20;
	} else {
		/* History is not completely filled */
		start_rev = 0;
		rev_nr = vfr->HEAD + 1;
	}
	PHOENIXFS_DBG("rename:: copying %d revisions", rev_nr);
	while (start_rev < rev_nr) {
		new_vfr->history[start_rev] = vfr->history[start_rev];
		start_rev = (start_rev + 1) % REV_TRUNCATE;
	}
	new_vfr->HEAD = rev_nr - 1;
	insert_vfr(dr, new_vfr);

	/* Remove old vfr */
	dr->vroot = remove_entry(dr->vroot, key);

	return 0;
}

static int phoenixfs_link(const char *path, const char *newpath)
{
	static char xnewpath[PATH_MAX];

	PHOENIXFS_DBG("link:: %s to %s", path, newpath);
	build_xpath(xpath, path, 0);
	build_xpath(xnewpath, newpath, 0);
	if (link(xpath, xnewpath) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_chmod(const char *path, mode_t mode)
{
	PHOENIXFS_DBG("chmod:: %s", path);
	build_xpath(xpath, path, 0);
	if (chmod(xpath, mode) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_chown(const char *path, uid_t uid, gid_t gid)
{
	/* chown is a no-op */
	return 0;
}

static int phoenixfs_truncate(const char *path, off_t newsize)
{
	PHOENIXFS_DBG("truncate:: %s to %lld", path, (long long int)newsize);
	build_xpath(xpath, path, 0);
	if (truncate(xpath, newsize) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_utime(const char *path, struct utimbuf *ubuf)
{
	PHOENIXFS_DBG("utime:: %s", path);
	build_xpath(xpath, path, 0);
	if (utime(xpath, ubuf) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_open(const char *path, struct fuse_file_info *fi)
{
	int rev, fd;
	FILE *infile, *fsfile;
	char fspath[PATH_MAX];
	struct file_record *fr;
	char sha1_digest[40];

	rev = parse_pathspec(xpath, path);
	build_xpath(fspath, xpath, 0);

	/* Skip zinflate for entries not in fstree */
	if (!(fr = find_fr(xpath, rev)))
		goto END;

	/* Build openpath by hand */
	print_sha1(sha1_digest, fr->sha1);
	sprintf(openpath, "%s/.git/loose/%s", ROOTENV->fsback, sha1_digest);
	if (access(openpath, F_OK) < 0) {
		/* Try extracting from packfile */
		sprintf(xpath, "%s/.git/loose", ROOTENV->fsback);
		if (unpack_entry(fr->sha1, xpath) < 0)
			return -ENOENT;
		else
			PHOENIXFS_DBG("open:: pack %s", sha1_digest);
	}
	else
		PHOENIXFS_DBG("open:: loose %s", sha1_digest);

	/* zinflate openpath onto fspath */
	PHOENIXFS_DBG("open:: zinflate %s onto %s", sha1_digest, fspath);
	if (!(infile = fopen(openpath, "rb")) ||
		!(fsfile = fopen(fspath, "wb+")))
		return -errno;
	if (zinflate(infile, fsfile) != Z_OK)
		PHOENIXFS_DBG("open:: zinflate issue");
	fclose(infile);
	rewind(fsfile);
END:
	if ((fd = open(fspath, fi->flags)) < 0)
		return -errno;
	fi->fh = fd;
	return 0;
}

static int phoenixfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	PHOENIXFS_DBG("mknod:: %s", path);
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
static int phoenixfs_create(const char *path, mode_t mode,
			struct fuse_file_info *fi)
{
	int fd;

	/* Always pass through to underlying filesystem */
	PHOENIXFS_DBG("create:: %s", path);
	build_xpath(xpath, path, 0);
	if ((fd = creat(xpath, mode)) < 0)
		return -errno;
	fi->fh = fd;

	return 0;
}

static int phoenixfs_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	ssize_t read_bytes;

	PHOENIXFS_DBG("read:: %s", path);
	if ((read_bytes = pread(fi->fh, buf, size, offset)) < 0)
		return -errno;
	return read_bytes;
}

static int phoenixfs_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	ssize_t written_bytes;

	PHOENIXFS_DBG("write:: %s", path);
	if ((written_bytes = pwrite(fi->fh, buf, size, offset)) < 0)
		return -errno;
	return written_bytes;
}

static int phoenixfs_statfs(const char *path, struct statvfs *statv)
{
	PHOENIXFS_DBG("statfs:: %s", path);
	build_xpath(xpath, path, 0);
	if (statvfs(xpath, statv) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_release(const char *path, struct fuse_file_info *fi)
{
	struct file_record *fr;
	FILE *infile, *outfile;
	struct stat st;
	unsigned char sha1[20];
	char sha1_digest[40];
	char outfilename[40];
	char inpath[PATH_MAX];
	char outpath[PATH_MAX];
	int rev, ret;

	/* Don't recursively backup history */
	if ((rev = parse_pathspec(xpath, path))) {
		PHOENIXFS_DBG("release:: history: %s", path);

		/* Inflate the original version back onto the filesystem */
		if (!(fr = find_fr(xpath, 0))) {
			PHOENIXFS_DBG("release:: Can't find revision 0!");
			return 0;
		}
		print_sha1(sha1_digest, fr->sha1);
		sprintf(inpath, "%s/.git/loose/%s", ROOTENV->fsback, sha1_digest);
		build_xpath(outpath, xpath, 0);

		if (!(infile = fopen(inpath, "rb")) ||
			!(outfile = fopen(outpath, "wb+")))
			return -errno;
		PHOENIXFS_DBG("release:: history: zinflate %s onto %s",
			sha1_digest, outpath);
		rewind(infile);
		rewind(outfile);
		if (zinflate(infile, outfile) != Z_OK)
			PHOENIXFS_DBG("release:: zinflate issue");
		fflush(outfile);
		fclose(infile);
		fclose(outfile);

		if (close(fi->fh) < 0) {
			PHOENIXFS_DBG("release:: can't really close");
			return -errno;
		}
		return 0;
	}

	/* Attempt to create a backup */
	build_xpath(xpath, path, 0);
	if ((infile = fopen(xpath, "rb")) < 0 ||
		(lstat(xpath, &st) < 0))
		return -errno;
	if ((ret = sha1_file(infile, st.st_size, sha1)) < 0)
		return ret;
	print_sha1(outfilename, sha1);
	sprintf(outpath, "%s/.git/loose/%s", ROOTENV->fsback, outfilename);
	if (!access(outpath, F_OK)) {
		/* SHA1 match; don't overwrite file as an optimization */
		PHOENIXFS_DBG("release:: not overwriting: %s", outpath);
		goto END;
	}
	if ((outfile = fopen(outpath, "wb")) < 0) {
		fclose(infile);
		return -errno;
	}

	/* Rewind and seek back */
	rewind(infile);
	PHOENIXFS_DBG("release:: zdeflate %s onto %s", xpath, outfilename);
	if (zdeflate(infile, outfile, -1) != Z_OK)
		PHOENIXFS_DBG("release:: zdeflate issue");
	mark_for_packing(sha1, st.st_size);
	fseek(infile, 0L, SEEK_END);
	fclose(outfile);
END:
	if (close(fi->fh) < 0) {
		PHOENIXFS_DBG("release:: can't really close");
		return -errno;
	}

	/* Update the fstree */
	fstree_insert_update_file(path, NULL);
	return 0;
}

static int phoenixfs_fsync(const char *path,
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

static int phoenixfs_ftruncate(const char *path,
			off_t offset, struct fuse_file_info *fi)
{
	build_xpath(xpath, path, 0);
	if (ftruncate(fi->fh, offset) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_readlink(const char *path, char *link, size_t size)
{
	/* Always pass through to underlying filesystem */
	PHOENIXFS_DBG("readlink:: %s", path);
	build_xpath(xpath, path, 0);
	if (readlink(xpath, link, size - 1) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_mkdir(const char *path, mode_t mode)
{
	build_xpath(xpath, path, 0);
	if (mkdir(xpath, mode) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_unlink(const char *path)
{
	/* Always pass through to underlying filesystem */
	PHOENIXFS_DBG("unlink:: %s", path);
	fstree_remove_file(path);
	build_xpath(xpath, path, 0);
	if (unlink(xpath) < 0)
		return -errno;
	return 0;
}

static int phoenixfs_rmdir(const char *path)
{
	/* Always pass through to underlying filesystem */
	PHOENIXFS_DBG("rmdir:: %s", path);
	build_xpath(xpath, path, 0);
	if (rmdir(xpath) < 0)
		return -errno;
	return 0;
}

static void phoenixfs_destroy(void *userdata)
{
	FILE *outfile;

	/* Persist the fstree */
	sprintf(xpath, "%s/.git/fstree", ROOTENV->fsback);
	if (!(outfile = fopen(xpath, "wb"))) {
		PHOENIXFS_DBG("destroy:: Can't open .git/fstree to persist");
		return;
	}
	PHOENIXFS_DBG("destroy:: dumping fstree");
	fstree_dump_tree(outfile);
	PHOENIXFS_DBG("destroy:: packing loose objects");
	sprintf(xpath, "%s/.git/loose", ROOTENV->fsback);
	dump_packing_info(xpath);
}

static struct fuse_operations phoenixfs_oper = {
	.init = phoenixfs_init,
	.getattr = phoenixfs_getattr,
	.fgetattr = phoenixfs_fgetattr,
	.open = phoenixfs_open,
	.mknod = phoenixfs_mknod,
	.releasedir = phoenixfs_releasedir,
	.create = phoenixfs_create,
	.read = phoenixfs_read,
	.write = phoenixfs_write,
	.statfs = phoenixfs_statfs,
	.access = phoenixfs_access,
	.getdir = NULL,
	.readdir = phoenixfs_readdir,
	.opendir = phoenixfs_opendir,
	.readlink = phoenixfs_readlink,
	.mkdir = phoenixfs_mkdir,
	.rmdir = phoenixfs_rmdir,
	.unlink = phoenixfs_unlink,
	.fsync = phoenixfs_fsync,
	.release = phoenixfs_release,
	.ftruncate = phoenixfs_ftruncate,
	.symlink = phoenixfs_symlink,
	.link = phoenixfs_link,
	.chown = phoenixfs_chown,
	.chmod = phoenixfs_chmod,
	.rename = phoenixfs_rename,
	.truncate = phoenixfs_truncate,
	.utime = phoenixfs_utime,
	.destroy = phoenixfs_destroy,
};

/* phoenixfs mount <path> <mountpoint> */
/* argv[2] is fsback and argv[3] is the mountpoint */
int phoenixfs_fuse(int argc, char *argv[])
{
	int nargc;
	char **nargv;
	FILE *infile;
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

	mkdir(xpath, S_IRUSR | S_IWUSR | S_IXUSR);
	if ((lstat(xpath, &st) < 0) ||
		(access(xpath, R_OK | W_OK | X_OK) < 0))
		die(".git doesn't have rwx permissions: %s", xpath);
	if (!S_ISDIR(st.st_mode))
		die(".git not a directory: %s", xpath);

	/* Check for .git/loose directory */
	sprintf(xpath, "%s/.git/loose", rootenv.fsback);

	mkdir(xpath, S_IRUSR | S_IWUSR | S_IXUSR);
	if ((lstat(xpath, &st) < 0) ||
		(access(xpath, R_OK | W_OK | X_OK) < 0))
		die(".git/loose doesn't have rwx permissions: %s", xpath);
	if (!S_ISDIR(st.st_mode))
		die(".git/loose not a directory: %s", xpath);

	PHOENIXFS_DBG("phoenixfs_fuse:: fsback: %s, mountpoint: %s",
		rootenv.fsback, rootenv.mountpoint);

	/* Check for .git/fstree to load tree */
	sprintf(xpath, "%s/.git/fstree", rootenv.fsback);
	if (!access(xpath, F_OK) &&
		(infile = fopen(xpath, "rb"))) {
		PHOENIXFS_DBG("phoenixfs_fuse:: loading fstree");
		fstree_load_tree(infile);
	}

	/* Check for .git/master.pack and .git/master.idx */
	sprintf(xpath, "%s/.git/master.pack", rootenv.fsback);
	sprintf(openpath, "%s/.git/master.idx", rootenv.fsback);
	if ((access(xpath, F_OK) < 0) ||
		(access(openpath, F_OK) < 0)) {
		PHOENIXFS_DBG("phoenixfs_fuse:: not loading packing info");
		load_packing_info(xpath, openpath, false);
	}
	else {
		PHOENIXFS_DBG("phoenixfs_fuse:: loading packing info");
		load_packing_info(xpath, openpath, 1);
	}

	nargv[0] = argv[0];
	nargv[1] = "-d";
	nargv[2] = "-odefault_permissions";
	nargv[3] = argv[3];
	return fuse_main(nargc, nargv, &phoenixfs_oper, &rootenv);
}
