#ifndef MAIN_H_
#define MAIN_H_

#include <time.h>
#include <stdio.h>
#include "common.h"
#include "diff.h"

enum subcmd {
	SUBCMD_NONE,
	SUBCMD_MOUNT,
	SUBCMD_LOG,
	SUBCMD_DIFF,
};

int gitfs_fuse(int argc, char *argv[]);

#endif
