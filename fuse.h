#ifndef FUSE_H_
#define FUSE_H_

#define FUSE_USE_VERSION 26
#define _XOPEN_SOURCE 500
#define GITFS_MAGIC 0x2888

#include <unistd.h>
#include <time.h>
#include <stdio.h>

#include "common.h"
#include "sha1.h"
#include "buffer.h"
#include "compress.h"
struct env_t {
	char *datapath;
	char *mountpoint;
	char *fsback;
	char *loosedir;
	time_t now;
};

int gitfs_fuse(int argc, char *argv[]);
void gitfs_setup(const char *datapath, const char *mountpoint,
		const char *fsback, const char *loosedir_suffix,
		struct env_t *private_ctx);

#define ROOTENV ((struct env_t *) fuse_get_context()->private_data)

#endif
