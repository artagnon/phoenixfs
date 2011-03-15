#include "pack.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *progname)
{
	die("Usage: %s <packfile>|-idx <packfile.idx>", progname);
}

int main(int argc, char* argv[])
{
	FILE *src;
	const char *srcfile;
	struct pack_header hdr;

	if (argc < 2 || (argc == 3 && strncmp(argv[1], "-idx", 4)))
		usage(argv[0]);
	srcfile = (argc  == 3 ? argv[2] : argv[1]);
	if (!(src = fopen(srcfile, "rb")))
		die("Could not open %s", argv[1]);
	return (argc == 3 ? map_pack_idx(src) :
		read_pack_header(src, &hdr));
}
