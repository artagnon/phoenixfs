#include "compress.h"

#include <string.h>
#include <stdio.h>
#include <zlib.h>
#include <errno.h>

int main(int argc, char* argv[])
{
	FILE *src;
	if (argc < 3)
		die("Usage: %s <i|d> <filename>", argv[0]);
	if (!(src = fopen(argv[2], "rb")))
		return -errno;
	if (!strcmp(argv[1], "i"))
		zinflate(src, stdout);
	else if (!strcmp(argv[1], "d"))
		zdeflate(src, stdout, -1);
	else
		die("Usage: %s <i|d> <filename>", argv[0]);
	fclose(src);
	return 0;
}
