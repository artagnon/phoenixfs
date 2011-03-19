#ifndef PERSIST_H_
#define PERSIST_H_

#include "common.h"
#include "btree.h"

#include <stdio.h>

void dump_dr_tree(struct node *root, FILE *outfile);
struct node *load_dr_tree(FILE *infile);

/* From fstree.c */
struct dir_record *make_dr(const char *path);
struct vfile_record *make_vfr(const char *path);

#endif
