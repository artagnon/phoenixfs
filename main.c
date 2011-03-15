#include "gitfs.h"
#include <ftw.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

static void usage(const char *progname, enum subcmd cmd)
{
	switch (cmd) {
	case SUBCMD_NONE:
		die("Usage: %s <subcommand> [arguments ...]", progname);
	case SUBCMD_INIT:
		die("Usage: %s mount <mountpoint> [packfile]", progname);
	case SUBCMD_LOG:
		die("Usage: %s log (unimplemented)", progname);
	case SUBCMD_DIFF:
		die("Usage: %s diff r1[:r2] <path> (unimplemented)", progname);
	default:
		die("Which subcommand?");
	}
}

static int unlink_cb(const char *path, const struct stat *st,
		int tflag, struct FTW *buf)
{
	if (remove(path) < 0)
		return -errno;
	return 0;
}

int main(int argc, char *argv[])
{
	int nargc = 4;
	char **nargv = (char **) malloc(nargc * sizeof(char *));
	char *fsback = "/tmp/gitfs_back";
	char *loosedir = "/tmp/gitfs_back/.loose";
	mode_t dirmode = 0777;
	struct env_t rootenv = {NULL, NULL, NULL, NULL, 0};

	nargv[0] = argv[0];
	nargv[1] = "-d";
	nargv[2] = "-odefault_permissions";
	if ((getuid() == 0) || (geteuid() == 0))
		die("Running gitfs as root opens unnacceptable security holes");

	if (argc < 2)
		usage(argv[0], SUBCMD_NONE);

	/* Pre init */
	if (!access(fsback, F_OK))
		/* rm -rf fsback */
		if (nftw(fsback, unlink_cb, 64, FTW_DEPTH | FTW_PHYS) < 0)
			die("Unable to remove %s", fsback);
	if (mkdir(fsback, dirmode) < 0)
		return -errno;
	if (mkdir(loosedir, dirmode) < 0)
		return -errno;

	/* Subcommand dispatch routine */
	if (!strncmp(argv[1], "mount", 5)) {
		if (argc < 3)
			usage(argv[0], SUBCMD_INIT);
		gitfs_subcmd_init(argv[2], argv[3], fsback,
				loosedir, &rootenv);
		nargv[3] = argv[3];
	} else if (!strncmp(argv[1], "log", 3))
		usage(argv[0], SUBCMD_LOG);
	else if (!strncmp(argv[1], "diff", 4))
		usage(argv[0], SUBCMD_DIFF);
	else
		usage(argv[0], SUBCMD_NONE);

	return gitfs_fuse(nargc, nargv, &rootenv);
}
