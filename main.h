#ifndef MAIN_H_
#define MAIN_H_

#include <time.h>
#include <stdio.h>
#include "common.h"
#include "buffer.h"
#include "diff.h"

#define DATE_LEN 26

enum subcmd {
	SUBCMD_NONE,
	SUBCMD_MOUNT,
	SUBCMD_LOG,
	SUBCMD_DIFF,
};

int phoenixfs_fuse(int argc, char *argv[]);

#endif
