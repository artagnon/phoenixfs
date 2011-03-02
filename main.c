#include "gitfs.h"
#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	if ((getuid() == 0) || (geteuid() == 0))
		die("Running gitfs as root opens unnacceptable security holes");

	return gitfs_fuse(argc, argv);
}
