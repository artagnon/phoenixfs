#include "main.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

static char path_buf[PATH_MAX] = "\0";

static void subcmd_diff(const char *pathspec1, const char *pathspec2)
{
	FILE *file1, *file2;
	struct stat st1, st2;

	if ((stat(pathspec1, &st1) < 0) ||
		(!(file1 = fopen(pathspec1, "rb"))))
		die("Could not open %s: %d", pathspec1, -errno);

	if ((stat(pathspec2, &st2) < 0) ||
		(!(file2 = fopen(pathspec2, "rb"))))
		die("Could not open %s: %d", pathspec2, -errno);

	phoenixfs_diff(file1, st1.st_size, file2, st2.st_size, stdout);
	fclose(file1);
	fclose(file2);
}

static void subcmd_log(const char *path)
{
	int rev;
	FILE *infile;
	struct tm *t;
	struct stat st;
	static char date_buf[DATE_LEN];
	static char mode_buf[20];

	for (rev = 0; ; rev++) {
		sprintf(path_buf, "%s@%d", path, rev);
		if ((stat(path_buf, &st) < 0) ||
			(!(infile = fopen(path_buf, "rb"))))
			break;
		/* Write header */
		rev ? printf("Revn:\tHEAD -%d\n", rev) : printf("Revn:\tHEAD\n");
		t = localtime(&st.st_mtime);
		strftime(date_buf, DATE_LEN, "%a, %d %b %Y %T", t);
		printf("Date:\t%s\n", date_buf);
		printf("Size:\t%lld bytes\n", (long long int)st.st_size);
		switch(st.st_mode) {
		case S_IFREG | 0755:
			strcpy(mode_buf, "Executable file");
			break;
		case S_IFLNK | 0644:
			strcpy(mode_buf, "Symbolic link");
			break;
		default:
			strcpy(mode_buf, "Regular file");
			break;
		}
		printf("Mode:\t%s\n\n", mode_buf);

		/* Write contents */
		buffer_copy_bytes(infile, stdout, st.st_size);
		printf("\n\n");
		fclose(infile);
	};
}

static void usage(const char *progname, enum subcmd cmd)
{
	switch (cmd) {
	case SUBCMD_NONE:
		die("Usage: %s <subcommand> [arguments ...]", progname);
	case SUBCMD_MOUNT:
		die("Usage: %s mount <gitdir> <mountpoint>", progname);
	case SUBCMD_LOG:
		die("Usage: %s log <filename>", progname);
	case SUBCMD_DIFF:
		die("Usage: %s diff <pathspec1> <pathspec2>", progname);
	default:
		die("Which subcommand?");
	}
}

int main(int argc, char *argv[])
{
	if ((getuid() == 0) || (geteuid() == 0))
		die("Running phoenixfs as root opens unnacceptable security holes");

	if (argc < 2)
		usage(argv[0], SUBCMD_NONE);

	/* Subcommand dispatch routine */
	if (!strncmp(argv[1], "mount", 5)) {
		if (argc < 3)
			usage(argv[0], SUBCMD_MOUNT);
		return phoenixfs_fuse(argc, argv);
	} else if (!strncmp(argv[1], "log", 3)) {
		if (argc < 2)
			usage(argv[0], SUBCMD_LOG);
		subcmd_log(argv[2]);
	}
	else if (!strncmp(argv[1], "diff", 4)) {
		if (argc < 4)
			usage(argv[0], SUBCMD_DIFF);
		subcmd_diff(argv[2], argv[3]);
	}
	else
		usage(argv[0], SUBCMD_NONE);
	return 0;
}
