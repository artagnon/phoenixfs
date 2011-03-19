#include "delta.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char* argv[])
{
	void *buf1, *buf2, *result_buf;
	unsigned long result_size;
	FILE *fh1, *fh2;
	struct stat st1, st2;

	buf1 = NULL;
	buf2 = NULL;
	result_buf = NULL;

	if (argc < 4)
		die("Usage: %s <c|a> <filename> <filename|delta>", argv[0]);
	if (!(fh1 = fopen(argv[2], "rb")) ||
		(lstat(argv[2], &st1) < 0) ||
		!(buf1 = malloc(st1.st_size)))
		return -errno;
	fread(buf1, st1.st_size, 1, fh1);
	if (!(fh2 = fopen(argv[3], "rb")) ||
		(lstat(argv[3], &st2) < 0) ||
		!(buf2 = malloc(st2.st_size)))
		return -errno;
	fread(buf2, st2.st_size, 1, fh2);
	if (!strcmp(argv[1], "c"))
		result_buf = diff_delta(buf1, st1.st_size, buf2,
					st2.st_size, &result_size, 0);
	else if (!strcmp(argv[1], "a"))
		result_buf = patch_delta(buf1, st1.st_size, buf2,
					st2.st_size, &result_size);
	else
		die("Usage: %s <c|a> <filename> <filename|delta>", argv[0]);
	fwrite(result_buf, result_size, 1, stdout);

	free(buf1);
	free(buf2);
	free(result_buf);
	fclose(fh1);
	fclose(fh2);
	return 0;
}
