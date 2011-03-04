#include "gitfs.h"
#include "diff.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static int load_mmfile(mmfile_t *mf_ptr, FILE *file, size_t sz)
{
	if (!(mf_ptr->ptr = malloc(sz)))
		return -errno;
	if (fread(mf_ptr->ptr, sz, 1, file) < 0)
		return -errno;
	mf_ptr->size = sz;
	return 0;
}

static int write_diff(void *file, mmbuffer_t *mb, int nbuf)
{
	int i;
	for (i = 0; i < nbuf; i++)
		fprintf((FILE *) file, "%.*s", (int) mb[i].size, mb[i].ptr);
	return 0;
}

int gitfs_diff(FILE* file1, size_t sz1, FILE* file2, size_t sz2, FILE* outfile)
{
	int ret;
	xdemitcb_t ecb;
	xpparam_t xpp;
	xdemitconf_t xecfg;
	mmfile_t mf1, mf2;

	if ((ret = load_mmfile(&mf1, file1, sz1)) < 0)
		return ret;
	if ((ret = load_mmfile(&mf2, file2, sz2) < 0))
		return ret;

	memset(&xpp, 0, sizeof(xpp));
	xpp.flags = 0;
	memset(&xecfg, 0, sizeof(xecfg));
	xecfg.ctxlen = 3;
	ecb.outf = write_diff;
	ecb.priv = outfile ? (void *) outfile : (void *) stdout;

	xdl_diff(&mf1, &mf2, &xpp, &xecfg, &ecb);

	free(mf1.ptr);
	free(mf2.ptr);

	return 0;
}
