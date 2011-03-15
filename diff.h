#ifndef DIFF_H_
#define DIFF_H_

#include <stdio.h>
#include "xdiff/xdiff.h"

int xdl_diff(mmfile_t *mf1, mmfile_t *mf2, xpparam_t const *xpp,
	     xdemitconf_t const *xecfg, xdemitcb_t *ecb);
int gitfs_diff(FILE* file1, size_t sz1, FILE* file2, size_t sz2, FILE* outfile);

#endif
