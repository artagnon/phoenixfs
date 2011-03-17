#include "fstree.h"

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static struct node *fsroot;

void build_xpath(char *xpath, const char *path)
{
	strcpy(xpath, ROOTENV->fsback);
	strcat(xpath, path);
}

/* filename is simply a pointer, while dirname is malloc'ed freshly */
int split_basename(const char *path, char *dirname, char *filename)
{
	int length;

	filename = strrchr(path, '/');
	length = strlen(path) - strlen(filename);
	/* Empty path is represented by '/' */
	length = (length ? length + 1 : 2);
	if (!(dirname = malloc((length) * sizeof(char))))
		return -1;
	strncpy(dirname, path, length - 1);
	dirname[length - 1] = '\0';
	GITFS_DBG("split_basename:: path: %s, dirname: %s, filename: %s",
		path, dirname, filename);
	return 0;
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
	/* st->st_uid = fuse_get_context()->uid; */
	/* st->st_gid = fuse_get_context()->gid; */
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
	uint16_t key = ~0;
	void *record;

	GITFS_DBG("find_dr:: %s", path);
	compute_crc32(key, (const unsigned char *) path, strlen(path));
	if (!(record = find(fsroot, key, 0)))
		return NULL;
	return ((struct dir_record *) record);
}

struct vfile_record *find_vfr(const char *path)
{
	uint16_t key = ~0;
	struct dir_record *dr;
	char *dirname, *filename;
	void *record;

	dirname = NULL;
	filename = NULL;
	split_basename(path, dirname, filename);
	if (!(dr = find_dr(dirname)) || !dr->vroot)
		return NULL;
	compute_crc32(key, (const unsigned char *) filename,
		(size_t) strlen((char *) filename));
	if (!(record = find(dr->vroot, key, 0)))
		return NULL;
	return ((struct vfile_record *) record);
}

struct file_record *find_fr(const char *path, int rev)
{
	struct vfile_record *vfr;
	struct file_record *record;

	if (!(vfr = find_vfr(path)))
		return NULL;
	if (rev < 0)
		rev = vfr->HEAD;
	if (!(record = vfr->history[rev]))
		return NULL;
	return record;
}

void insert_dr(struct dir_record *dr)
{
	uint16_t key = ~0;

	compute_crc32(key, (const unsigned char *) dr->name,
		(size_t) strlen((char *) dr->name));
	fsroot = insert(fsroot, key, dr);
}

void insert_vfr(struct dir_record *dr, struct vfile_record *vfr)
{
	uint16_t key = ~0;

	compute_crc32(key, (const unsigned char *) vfr->name,
		(size_t) strlen((char *) vfr->name));
	dr->vroot = insert(dr->vroot, key, vfr);
}

void insert_fr(struct vfile_record *vfr, struct file_record *fr)
{
	int newHEAD;

	newHEAD = (vfr->HEAD + 1) % REV_TRUNCATE;
	vfr->history[newHEAD] = fr;
	vfr->HEAD = newHEAD;
}

struct dir_record *make_dr(const char *path)
{
	struct dir_record *dr;

	GITFS_DBG("make_dr:: %s", path);
	if (!(dr = malloc(sizeof(struct dir_record))))
		return NULL;
	memcpy(dr->name, path, strlen(path));
	dr->vroot = NULL;
	return dr;
}

struct vfile_record *make_vfr(const char *name)
{
	struct vfile_record *vfr;

	GITFS_DBG("make_vfr:: %s", name);
	if (!(vfr = malloc(sizeof(struct vfile_record))))
		return NULL;
	memcpy(vfr->name, name, strlen(name));
	vfr->HEAD = -1;
	return vfr;
}

struct file_record *make_fr(const char *path)
{
	struct file_record *fr;
	unsigned char sha1[20];
	struct stat st;
	FILE *infile;

	GITFS_DBG("make_fr:: %s", path);
	if (!(fr = malloc(sizeof(struct file_record))))
		return NULL;
	if (!(infile = fopen(path, "rb")) ||
		(stat(path, &st) < 0) ||
		(sha1_file(infile, st.st_size, sha1) < 0)) {
		fclose(infile);
		return NULL;
	}
	fclose(infile);
	memcpy(fr->sha1, sha1, 20);
	fill_fr(fr, &st);
	return fr;
}

void ls_dir(const char *path, struct node *root, struct node *iter)
{
	struct dir_record *dr;

	if (!(dr = find_dr(path)) || !dr->vroot) {
		root = NULL;
		iter = NULL;
		return;
	}
	root = dr->vroot;
	iter = dr->vroot;

	/* Return only the leaves */
	while (!iter->is_leaf)
		iter = iter->pointers[0];
}

void insert_update_file(const char *path)
{
	struct dir_record *dr;
	struct vfile_record *vfr;
	struct file_record *fr;
	char *dirname, *filename;

	dirname = NULL;
	filename = NULL;
	split_basename(path, dirname, filename);
	if (!(dr = find_dr(dirname)))
		goto DR;
	else {
		if (!(vfr = find_vfr(path)))
			goto VFR;
		else {
			fr = find_fr(path, -1);
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
	fr = make_fr(path);
	insert_fr(vfr, fr);
}

void print_fstree(void)
{
	node *n = NULL;
	int i = 0;
	int rank = 0;
	int new_rank = 0;
	struct node *queue;
	FILE *rootlog_fh;

	rootlog_fh = fopen("/tmp/gitfs.log", "a");
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
