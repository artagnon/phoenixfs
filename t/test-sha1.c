#include "gitfs.h"
#include "sha1.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

static void usage(const char *progname)
{
	die("Usage: %s <file>", progname);
}

int main(int argc, char* argv[])
{
	int i;
	FILE *infile;
	unsigned char sha1[20];
	struct stat st;

	if (argc < 2)
		usage(argv[0]);
	if (!(infile = fopen(argv[1], "rb")))
		die("Could not open %s", argv[1]);
	if (stat(argv[1], &st) < 0)
		die("Could not stat %s", argv[1]);
	if (sha1_file(infile, st.st_size, sha1) < 0)
		die("SHA1 failed");
	fclose(infile);
	for (i = 0; i < 20; i++)
		printf("%02x", sha1[i]);
	printf("  %s\n", argv[1]);
	return 0;
}
