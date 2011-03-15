#include "main.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

static void subcmd_log(const char *pathspec)
{
	return;
}

static void subcmd_diff(const char *pathspec1, const char *pathspec2)
{
	FILE *file1, *file2;
	struct stat st1, st2;

	if (stat(pathspec1, &st1))
		die("Could not stat %s", pathspec1);
	if (!(file1 = fopen(pathspec1, "rb")))
		die("Could not open %s", pathspec1);

	if (stat(pathspec2, &st2))
		die("Could not stat %s", pathspec2);
	if (!(file2 = fopen(pathspec2, "rb")))
		die("Could not open %s", pathspec2);

	gitfs_diff(file1, st1.st_size, file2, st2.st_size, stdout);
	fclose(file1);
	fclose(file2);
}

static void usage(const char *progname, enum subcmd cmd)
{
	switch (cmd) {
	case SUBCMD_NONE:
		die("Usage: %s <subcommand> [arguments ...]", progname);
	case SUBCMD_MOUNT:
		die("Usage: %s mount <packfile> <mountpoint>", progname);
	case SUBCMD_LOG:
		die("Usage: %s log (unimplemented)", progname);
	case SUBCMD_DIFF:
		die("Usage: %s diff <pathspec1> <pathspec2>", progname);
	default:
		die("Which subcommand?");
	}
}

int main(int argc, char *argv[])
{
	if ((getuid() == 0) || (geteuid() == 0))
		die("Running gitfs as root opens unnacceptable security holes");

	if (argc < 2)
		usage(argv[0], SUBCMD_NONE);

	/* Subcommand dispatch routine */
	if (!strncmp(argv[1], "mount", 5)) {
		if (argc < 3)
			usage(argv[0], SUBCMD_MOUNT);
		return gitfs_fuse(argc, argv);
	} else if (!strncmp(argv[1], "log", 3)) {
		if (argc < 2)
			usage(argv[0], SUBCMD_LOG);
		subcmd_log(argv[2]);
	}
	else if (!strncmp(argv[1], "diff", 4)) {
		if (argc < 3)
			usage(argv[0], SUBCMD_DIFF);
		subcmd_diff(argv[2], argv[3]);
	}
	else
		usage(argv[0], SUBCMD_NONE);
	return 0;
}
