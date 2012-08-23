#include "fstree.h"

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static struct node *fsroot = NULL;
static char dirname[PATH_MAX] = "\0";

/* Pathspec: <path>[@<rev>] */
int parse_pathspec(char *xpath, const char *path)
{
	int revision;
	char *split;

	if (!(split = strrchr(path, '@')))
		goto END;
	revision = atoi(split + 1);
	if (revision < 0 || revision > 20)
		goto END;
	memcpy(xpath, path, split - path);
	xpath[split - path] = '\0';
	return revision;
END:
	strcpy(xpath, path);
	return 0;
}

int build_xpath(char *xpath, const char *path, int rev)
{
	struct file_record *fr;
	char sha1_digest[40];

	if (!rev) {
		/* Search on FS */
		strcpy(xpath, ROOTENV->fsback);
		strcat(xpath, path);
		return 0;
	}
	if (!(fr = find_fr(path, rev))) {
		PHOENIXFS_DBG("build_xpath:: missing: %s@%d", path, rev);
		/* Might be a directory; copy and give to caller */
		strcpy(xpath, path);
		return -1;
	}
	print_sha1(sha1_digest, fr->sha1);
	sprintf(xpath, "%s/.git/loose/%s", ROOTENV->fsback, sha1_digest);
	if (access(xpath, F_OK) < 0) {
		/* Try extracting from packfile */
		sprintf(xpath, "%s/.git/loose", ROOTENV->fsback);
		if (unpack_entry(fr->sha1, xpath) < 0)
			return -1;
		else
			PHOENIXFS_DBG("open:: pack %s", sha1_digest);
	}
	else
		PHOENIXFS_DBG("open:: loose %s", sha1_digest);

	return 0;
}

/* filename is simply a pointer; dirname must have alloc'ed memory */
char *split_basename(const char *path, char *dirname)
{
	int length;
	char *filename;

	/* In the worst case, strrchr returns 0: leading '/' */
	filename = strrchr(path, '/') + 1;

	/* Strip trailing '/' from all directories except '/' itself */
	/* +1 to accomodate the null terminator */
	length = strlen(path) - strlen(filename);
	length = (length == 1 ? 2 : length);

	if (!dirname) {
		PHOENIXFS_DBG("split_basename:: path: %s, filename: %s",
			path, filename);
		return filename;
	}

	memcpy(dirname, path, length - 1);
	dirname[length - 1] = '\0';

	PHOENIXFS_DBG("split_basename:: path: %s, dirname: %s, filename: %s",
		path, dirname, filename);
	return filename;
}

void fill_stat(struct stat *st, struct file_record *fr)
{
	if (!fr || !st) {
		st = NULL;
		return;
	}
	memset(st, 0, sizeof(struct stat));
	switch (fr->mode) {
	case NODE_EXECUTABLE:
		st->st_mode = S_IFREG | 0755;
		break;
	case NODE_SYMLINK:
		st->st_mode = S_IFLNK | 0644;
		break;
	default:
		st->st_mode = S_IFREG | 0644;
	}
	st->st_nlink = 1;
	st->st_mtime = fr->mtime;
	st->st_size = fr->size;
}

void fill_fr(struct file_record *fr, struct stat *st)
{
	switch(st->st_mode) {
	case S_IFREG | 0755:
		fr->mode = NODE_EXECUTABLE;
		break;
	case S_IFLNK | 0644:
		fr->mode = NODE_SYMLINK;
		break;
	default:
		fr->mode = NODE_REGULAR;
		break;
	}
	fr->size = st->st_size;
	fr->mtime = st->st_mtime;
}

struct dir_record *find_dr(const char *path)
{
	struct dir_record *dr;
	uint16_t key = ~0;
	size_t length;

	length = (size_t) strlen((char *) path);
	key = compute_crc32(key, (const unsigned char *) path, length);
	if (!(dr = find(fsroot, key, 0))) {
		PHOENIXFS_DBG("find_dr:: missing %s", path);
		return NULL;
	}
	PHOENIXFS_DBG("find_dr:: found %s", path);
	return dr;
}

struct vfile_record *find_vfr(const char *path)
{
	uint16_t key = ~0;
	struct dir_record *dr;
	struct vfile_record *vfr;
	char *filename;
	size_t length;

	filename = split_basename(path, dirname);
	if (!(dr = find_dr(dirname)) || !dr->vroot) {
		PHOENIXFS_DBG("find_vfr:: not found %s", path);
		return NULL;
	}
	length = (size_t) strlen((char *) filename);
	key = compute_crc32(key, (const unsigned char *) filename, length);
	if (!(vfr = find(dr->vroot, key, 0))) {
		PHOENIXFS_DBG("find_vfr:: not found %s", path);
		return NULL;
	}
	PHOENIXFS_DBG("find_vfr:: found %s", path);
	return vfr;
}

struct file_record *find_fr(const char *path, int rev)
{
	struct vfile_record *vfr;
	struct file_record *fr;

	if (!(vfr = find_vfr(path))) {
		PHOENIXFS_DBG("find_fr:: not found %s", path);
		return NULL;
	}
	/* Translate rev to mean "number of revs before HEAD" */
	rev = (vfr->HEAD - rev) % REV_TRUNCATE;
	if (!(fr = vfr->history[rev])) {
		PHOENIXFS_DBG("find_fr:: not found %s", path);
		return NULL;
	}
	PHOENIXFS_DBG("find_fr:: found %s", path);
	return fr;
}

void insert_dr(struct dir_record *dr)
{
	uint16_t key = ~0;
	size_t length;

	length = (size_t) strlen((char *) dr->name);
	key = compute_crc32(key, (const unsigned char *) dr->name, length);
	PHOENIXFS_DBG("insert_dr:: %08X", key);
	fsroot = insert(fsroot, key, dr);
}

void insert_vfr(struct dir_record *dr, struct vfile_record *vfr)
{
	uint16_t key = ~0;
	size_t length;

	length = (size_t) strlen((char *) vfr->name);
	key = compute_crc32(key, (const unsigned char *) vfr->name, length);
	PHOENIXFS_DBG("insert_vfr:: %08X", key);
	dr->vroot = insert(dr->vroot, key, vfr);
}

void insert_fr(struct vfile_record *vfr, struct file_record *fr)
{
	int newHEAD;

	newHEAD = (vfr->HEAD + 1) % REV_TRUNCATE;
	vfr->history[newHEAD] = fr;
	vfr->HEAD = newHEAD;
	PHOENIXFS_DBG("insert_fr:: %s [%d]", vfr->name, vfr->HEAD);
}

struct node *remove_entry(struct node *root, uint16_t key)
{
	return delete(root, key);
}

struct dir_record *make_dr(const char *path)
{
	struct dir_record *dr;

	PHOENIXFS_DBG("make_dr:: %s", path);
	if (!(dr = malloc(sizeof(struct dir_record))))
		return NULL;
	memcpy(dr->name, path, strlen(path) + 1);
	dr->vroot = NULL;
	return dr;
}

struct vfile_record *make_vfr(const char *name)
{
	struct vfile_record *vfr;

	PHOENIXFS_DBG("make_vfr:: %s", name);
	if (!(vfr = malloc(sizeof(struct vfile_record))))
		return NULL;
	memcpy(vfr->name, name, strlen(name) + 1);
	memset(vfr->history, 0,
		REV_TRUNCATE * sizeof(struct file_record *));
	vfr->HEAD = -1;
	return vfr;
}

struct file_record *make_fr(const char *path, const char *follow)
{
	struct file_record *fr;
	unsigned char sha1[20];
	char xpath[PATH_MAX];
	struct stat st;
	FILE *infile;

	PHOENIXFS_DBG("make_fr:: %s", path);
	if (!(fr = malloc(sizeof(struct file_record))))
		return NULL;
	build_xpath(xpath, path, 0);

	if (lstat(xpath, &st) < 0) {
		PHOENIXFS_DBG("make_fr:: can't stat %s", xpath);
		free(fr);
		return NULL;
	}

	/* No point computing SHA1 of symlinks */
	if (S_ISLNK(st.st_mode)) {
		PHOENIXFS_DBG("make_fr:: link %s to %s", path, follow);
		memset(fr->sha1, 0, 20);
		strcpy((char *) fr->follow, follow);
		goto END;
	}

	/* Compute SHA1 of regular and executable files */
	if (!(infile = fopen(xpath, "rb")) ||
		(sha1_file(infile, st.st_size, sha1) < 0)) {
		free(fr);
		return NULL;
	}
	fclose(infile);
	memcpy(fr->sha1, sha1, 20);
	strcpy((char *) fr->follow, "\0");
END:
	fill_fr(fr, &st);
	return fr;
}

void fstree_insert_update_file(const char *path, const char *follow)
{
	struct dir_record *dr;
	struct vfile_record *vfr;
	struct file_record *fr = NULL, *new_fr;
	uint16_t key = ~0;
	char *filename;
	size_t length;

	filename = split_basename(path, dirname);
	if (!(dr = find_dr(dirname)))
		goto DR;
	else {
		length = (size_t) strlen((char *) filename);
		key = compute_crc32(key, (const unsigned char *) filename, length);
		if (!(vfr = find(dr->vroot, key, 0))) {
			PHOENIXFS_DBG("fstree_insert_update_file:: missing vfr: %s", filename);
			goto VFR;
		}
		else {
			if (vfr->HEAD >= 0)
				fr = vfr->history[vfr->HEAD];
			goto FR;
		}
	}
DR:
	dr = make_dr(dirname);
	insert_dr(dr);
VFR:
	vfr = make_vfr(filename);
	insert_vfr(dr, vfr);
FR:
	if (!(new_fr = make_fr(path, follow))) {
		PHOENIXFS_DBG("fstree_insert_update_file:: Can't make fr %s", path);
		return;
	}

	/* If content is present in the old fr, don't make a new fr */
	if (fr && !memcmp(fr->sha1, new_fr->sha1, 20)) {
		PHOENIXFS_DBG("fstree_insert_update_file:: unmodified: %s", path);
		free(new_fr);
		return;
	}
	insert_fr(vfr, new_fr);
}

void fstree_remove_file(const char *path)
{
	struct dir_record *dr;
	struct vfile_record *vfr;
	uint16_t key = ~0;
	size_t length;
	char *filename;
	int rev;

	filename = split_basename(path, dirname);
	length = (size_t) strlen((char *) filename);
	key = compute_crc32(key, (const unsigned char *) filename, length);

	if (!(dr = find_dr(dirname))) {
		PHOENIXFS_DBG("fstree_remove_file:: missing %s", dirname);
		return;
	}
	if (!(vfr = find(dr->vroot, key, 0))) {
		PHOENIXFS_DBG("fstree_remove_file:: missing %s", filename);
		return;
	}
	for (rev = 0; ; rev++) {
		if (!vfr->history[rev])
			break;
		free(vfr->history[rev]);
	}
	PHOENIXFS_DBG("fstree_remove_file:: %s", path);
	delete(dr->vroot, key);
}

void fstree_dump_tree(FILE *outfile)
{
	dump_dr_tree(fsroot, outfile);
}

void fstree_load_tree(FILE *infile)
{
	fsroot = load_dr_tree(infile);
}

void print_fstree(void)
{
	node *n = NULL;
	int i = 0;
	int rank = 0;
	int new_rank = 0;
	struct node *queue;
	FILE *rootlog_fh;

	rootlog_fh = fopen("/tmp/phoenixfs.log", "a");
	if (fsroot == NULL) {
		fprintf(rootlog_fh, "Empty tree.\n");
		return;
	}
	queue = NULL;
	enqueue(fsroot);
	while (queue != NULL) {
		n = dequeue();
		if (n->parent != NULL && n == n->parent->pointers[0]) {
			new_rank = path_to_root (fsroot, n);
			if (new_rank != rank) {
				rank = new_rank;
				fprintf(rootlog_fh, "\n");
			}
		}
		fprintf(rootlog_fh, "(%lx)", (unsigned long) n);
		for (i = 0; i < n->num_keys; i++) {
			fprintf(rootlog_fh, "%lx ", (unsigned long) n->pointers[i]);
			fprintf(rootlog_fh, "%d ", n->keys[i]);
		}
		if (!n->is_leaf)
			for (i = 0; i <= n->num_keys; i++)
				enqueue(n->pointers[i]);
		if (n->is_leaf)
			fprintf(rootlog_fh, "%lx ", (unsigned long) n->pointers[BTREE_ORDER - 1]);
		else
			fprintf(rootlog_fh, "%lx ", (unsigned long) n->pointers[n->num_keys]);
		fprintf(rootlog_fh, "| ");
	}
	fprintf(rootlog_fh, "\n");
	fclose(rootlog_fh);
}
