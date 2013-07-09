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
	FILE *infile;
	unsigned char sha1[20];
	char sha1_digest[40];
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
	print_sha1(sha1_digest, sha1);
	printf("%s\n", sha1_digest);
	return 0;
}
