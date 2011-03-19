#include "persist.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

unsigned char path_buf[PATH_MAX] = "\0";

/* --------------
 * The dump part
 * --------------
 */
/**
 * Format:
 * <> :=
 * (struct file_record)
 */
static void dump_frs(struct vfile_record *vfr, uint8_t start_rev,
		uint8_t num_revs, FILE *outfile)
{
	while (start_rev < num_revs) {
		fwrite(vfr->history[start_rev],
			sizeof(struct file_record), 1, outfile);
		GITFS_DBG("dump_frs:: %s [%u]", vfr->name, start_rev);
		start_rev = (start_rev + 1) % REV_TRUNCATE;
	}
}

/**
 * Format:
 * <> :=
 * num_keys | [key | name_len | name | num_revs | [<dump_frs>][...]][...]
 */
static void dump_vfr_tree(struct node *root, FILE *outfile)
{
	struct vfile_record *vfr;
	uint8_t start_rev, num_revs;
	uint16_t name_len;
	register int i;
	node *iter;

	iter = root;
	while (!iter->is_leaf)
		iter = iter->pointers[0];
	fwrite(&(iter->num_keys), sizeof(uint16_t), 1, outfile);

	while (1) {
		for (i = 0; i < iter->num_keys; i++) {
			/* Write the key */
			fwrite(&(iter->keys[i]), sizeof(uint16_t), 1, outfile);
			vfr = find(root, iter->keys[i], 0);

			/* Compute name_len; write name_len and name */
			name_len = strlen((const char *) vfr->name);
			fwrite(&name_len, sizeof(uint16_t), 1, outfile);
			fwrite(vfr->name, name_len * sizeof(unsigned char), 1, outfile);

			/* Compute and write num_revs and HEAD */
			if (vfr->HEAD < 0) {
				start_rev = 0;
				num_revs = 0;
			} else if (vfr->history[(vfr->HEAD + 1) % REV_TRUNCATE]) {
				/* History is full, and is probably wrapping around */
				start_rev = (vfr->HEAD + 1) % REV_TRUNCATE;
				num_revs = 20;
			} else {
				/* History is not completely filled */
				start_rev = 0;
				num_revs = vfr->HEAD + 1;
			}
			fwrite(&num_revs, sizeof(uint8_t), 1, outfile);

			/* Write the actual file records in chronological order */
			dump_frs(vfr, start_rev, num_revs, outfile);
		}
		if (iter->pointers[BTREE_ORDER - 1] != NULL)
			iter = iter->pointers[BTREE_ORDER - 1];
		else
			break;
	}
}

/**
 * Format:
 * <> :=
 * num_keys | [key | name_len | name | [<dump_vfr>][...]][...]
 */
void dump_dr_tree(struct node *root, FILE *outfile)
{
	struct dir_record *dr;
	uint16_t name_len;
	register int i;
	node *iter;

	iter = root;
	while (!iter->is_leaf)
		iter = iter->pointers[0];
	fwrite(&(iter->num_keys), sizeof(uint16_t), 1, outfile);

	while (1) {
		for (i = 0; i < iter->num_keys; i++) {
			/* Write the key */
			fwrite(&(iter->keys[i]), sizeof(uint16_t), 1, outfile);
			dr = find(root, iter->keys[i], 0);

			/* Compute name_len; write name_len and name */
			name_len = strlen((const char *) dr->name);
			fwrite(&name_len, sizeof(uint16_t), 1, outfile);
			fwrite(dr->name, name_len * sizeof(unsigned char), 1, outfile);

			dump_vfr_tree(dr->vroot, outfile);
			GITFS_DBG("dump_dr_tree:: %s", (const char *) dr->name);
		}
		if (iter->pointers[BTREE_ORDER - 1] != NULL)
			iter = iter->pointers[BTREE_ORDER - 1];
		else
			break;
	}
}


/* --------------
 * The load part
 * --------------
 */
/**
 * Format:
 * <> :=
 * num_keys | [key | name_len | name | num_revs | [<load_frs>][...]][...]
 */
struct node *load_vfr_tree(FILE *infile)
{
	struct node *root;
	struct vfile_record *vfr;
	uint16_t key;
	uint16_t num_keys;
	uint16_t name_len;
	uint8_t num_revs;
	register int i, j;

	root = NULL;
	fread(&num_keys, sizeof(uint16_t), 1, infile);
	for (i = 0; i < num_keys; i++) {
		fread(&key, sizeof(uint16_t), 1, infile);
		fread(&name_len, sizeof(uint16_t), 1, infile);
		fread(&path_buf, name_len * sizeof(unsigned char), 1, infile);
		fread(&num_revs, sizeof(uint8_t), 1, infile);
		vfr = make_vfr((const char *) path_buf);
		root = insert(root, key, (void *) vfr);
		for (j = 0; j < num_revs; j++) {
			vfr->history[j] = malloc(sizeof(struct file_record));
			fread(vfr->history[j], sizeof(struct file_record), 1, infile);
			GITFS_DBG("load_vfr_tree:: %s [%d]", vfr->name, j);
		}
		vfr->HEAD = num_revs - 1;
	}
	return root;
}

/**
 * Format:
 * <> :=
 * num_keys | [key | name_len | name | [<load_vfr>][...]][...]
 */
struct node *load_dr_tree(FILE *infile)
{
	struct node *root;
	struct dir_record *dr;
	uint16_t key;
	uint16_t num_keys;
	uint16_t name_len;
	register int i;

	root = NULL;
	fread(&num_keys, sizeof(uint16_t), 1, infile);
	for (i = 0; i < num_keys; i++) {
		fread(&key, sizeof(uint16_t), 1, infile);
		fread(&name_len, sizeof(uint16_t), 1, infile);
		fread(&path_buf, name_len * sizeof(unsigned char), 1, infile);
		GITFS_DBG("load_dr_tree:: %s", (const char *) path_buf);
		dr = make_dr((const char *) path_buf);
		root = insert(root, key, (void *) dr);
		dr->vroot = load_vfr_tree(infile);
	}
	return root;
}
