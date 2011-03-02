#include "gitfs.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static void vreportf(const char *fmt, va_list params)
{
	char msg[4096];
	vsnprintf(msg, sizeof(msg), fmt, params);
	fprintf(stderr, "%s\n", msg);
}

void die(const char *fmt, ...)
{
	va_list params;
	va_start(params, fmt);
	vreportf(fmt, params);
	va_end(params);
	exit(128);
}

static void usage(const char *progname, enum subcmd cmd)
{
	switch (cmd) {
	case SUBCMD_NONE:
		die("Usage: %s <subcommand> [arguments ...]", progname);
	case SUBCMD_INIT:
		die("Usage: %s init <mountpoint> [packfile]", progname);
	case SUBCMD_LOG:
		die("Usage: %s log (unimplemented)", progname);
	case SUBCMD_DIFF:
		die("Usage: %s diff r1[:r2] <path> (unimplemented)", progname);
	default:
		die("Which subcommand?");
	}
}

int main(int argc, char *argv[])
{
	int nargc = 4;
	char **nargv = (char **) malloc(nargc * sizeof(char *));

	nargv[0] = argv[0];
	nargv[1] = "-d";
	nargv[2] = "-odefault_permissions";
	if ((getuid() == 0) || (geteuid() == 0))
		die("Running gitfs as root opens unnacceptable security holes");

	if (argc < 2)
		usage(argv[0], SUBCMD_NONE);

	/* Subcommand dispatch routine */
	if (!strncmp(argv[1], "init", 4)) {
		if (argc < 3)
			usage(argv[0], SUBCMD_INIT);
		gitfs_subcmd_init(argv[2], (argc == 4 ? argv[3] : NULL));
		nargv[3] = argv[2];
	} else if (!strncmp(argv[1], "log", 3))
		usage(argv[0], SUBCMD_LOG);
	else if (!strncmp(argv[1], "diff", 4))
		usage(argv[0], SUBCMD_DIFF);
	else
		usage(argv[0], SUBCMD_NONE);

	return gitfs_fuse(nargc, nargv);
}
