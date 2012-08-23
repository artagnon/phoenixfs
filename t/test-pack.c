#include "pack.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *progname)
{
	die("Usage: %s <packfile> <packfile.idx> <sha1>", progname);
}

int main(int argc, char* argv[])
{
	unsigned char sha1[20];

	if (argc < 4)
		usage(argv[0]);
	load_packing_info(argv[1], argv[2], true);
	get_sha1_hex(argv[3], sha1);
	return unpack_entry(sha1, ".");
}
