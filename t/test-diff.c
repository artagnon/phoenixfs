#include "gitfs.h"
#include "diff.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static void usage(const char *progname)
{
	die("Usage: %s <file1> <file2> [outfile]", progname);
}

int main(int argc, char* argv[])
{
	FILE *file1, *file2, *outfile;
	size_t sz1, sz2;
	struct stat st1, st2;

	if (argc < 3)
		usage(argv[0]);
	if (stat(argv[1], &st1))
		die("Could not stat %s", argv[1]);
	if (!(file1 = fopen(argv[1], "rb")))
		die("Could not open %s", argv[1]);
	sz1 = (size_t)(st1.st_size);

	if (stat(argv[2], &st2))
		die("Could not stat %s", argv[2]);
	if (!(file2 = fopen(argv[2], "rb")))
		die("Could not open %s", argv[2]);
	sz2 = (size_t)(st2.st_size);

	outfile = stdout;
	if (argc > 3) {
		if (!(outfile = fopen(argv[3], "w")))
			die("Could not open %s", argv[3]);
	}

	gitfs_diff(file1, sz1, file2, sz2, outfile);
	fclose(file1);
	fclose(file2);
	if (outfile)
		fclose(outfile);
	return 0;
}
