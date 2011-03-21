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
		uint8_t rev_nr, FILE *outfile)
{
	while (start_rev < rev_nr) {
		fwrite(vfr->history[start_rev],
			sizeof(struct file_record), 1, outfile);
		PHOENIXFS_DBG("dump_frs:: %s [%u]", vfr->name, start_rev);
		start_rev = (start_rev + 1) % REV_TRUNCATE;
	}
}

/**
 * Format:
 * <> :=
 * num_keys | [key | name_len | name | rev_nr | [<dump_frs>][...]][...]
 */
static void dump_vfr_tree(struct node *root, FILE *outfile)
{
	struct vfile_record *vfr;
	uint8_t start_rev, rev_nr;
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
			PHOENIXFS_DBG("dump_vfr_tree:: vfr %s", (const char *) vfr->name);

			/* Compute and write rev_nr and HEAD */
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
			fwrite(&rev_nr, sizeof(uint8_t), 1, outfile);

			/* Write the actual file records in chronological order */
			dump_frs(vfr, start_rev, rev_nr, outfile);
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
			PHOENIXFS_DBG("dump_dr_tree:: %s", (const char *) dr->name);

			dump_vfr_tree(dr->vroot, outfile);
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
 * num_keys | [key | name_len | name | rev_nr | [<load_frs>][...]][...]
 */
struct node *load_vfr_tree(FILE *infile)
{
	struct node *root;
	struct vfile_record *vfr;
	uint16_t key;
	uint16_t num_keys;
	uint16_t name_len;
	uint8_t rev_nr;
	register int i, j;

	root = NULL;
	memset(&num_keys, 0, sizeof(uint16_t));
	fread(&num_keys, sizeof(uint16_t), 1, infile);
	for (i = 0; i < num_keys; i++) {
		memset(&key, 0, sizeof(uint16_t));
		fread(&key, sizeof(uint16_t), 1, infile);
		memset(&name_len, 0, sizeof(uint16_t));
		fread(&name_len, sizeof(uint16_t), 1, infile);
		memset(&path_buf, 0, PATH_MAX);
		fread(&path_buf, name_len * sizeof(unsigned char), 1, infile);
		memset(&rev_nr, 0, sizeof(uint8_t));
		fread(&rev_nr, sizeof(uint8_t), 1, infile);
		vfr = make_vfr((const char *) path_buf);
		root = insert(root, key, (void *) vfr);
		for (j = 0; j < rev_nr; j++) {
			vfr->history[j] = malloc(sizeof(struct file_record));
			memset(vfr->history[j], 0, sizeof(struct file_record));
			fread(vfr->history[j], sizeof(struct file_record), 1, infile);
			PHOENIXFS_DBG("load_vfr_tree:: %s [%d]", vfr->name, j);
		}
		vfr->HEAD = rev_nr - 1;
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
	memset(&num_keys, 0, sizeof(uint16_t));
	fread(&num_keys, sizeof(uint16_t), 1, infile);
	for (i = 0; i < num_keys; i++) {
		memset(&key, 0, sizeof(uint16_t));
		fread(&key, sizeof(uint16_t), 1, infile);
		memset(&name_len, 0, sizeof(uint16_t));
		fread(&name_len, sizeof(uint16_t), 1, infile);
		memset(&path_buf, 0, PATH_MAX);
		fread(&path_buf, name_len * sizeof(unsigned char), 1, infile);
		PHOENIXFS_DBG("load_dr_tree:: %s", (const char *) path_buf);
		dr = make_dr((const char *) path_buf);
		root = insert(root, key, (void *) dr);
		dr->vroot = load_vfr_tree(infile);
	}
	return root;
}
