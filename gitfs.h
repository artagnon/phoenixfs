#ifndef GITFS_H_
#define GITFS_H_

#define FUSE_USE_VERSION 26
#define _XOPEN_SOURCE 500
#define GITFS_MAGIC 0x2888

#include <unistd.h>
#include <time.h>
#include <stdio.h>

struct env_t {
	char *datapath;
	char *mountpoint;
	char *fsback;
	char *loosedir;
	time_t now;
};

enum subcmd {
	SUBCMD_NONE,
	SUBCMD_INIT,
	SUBCMD_LOG,
	SUBCMD_DIFF,
};

void die(const char *err, ...);
int gitfs_fuse(int argc, char *argv[], struct env_t *private_ctx);
void gitfs_subcmd_init(const char *datapath, const char *mountpoint,
		const char *fsback, const char *loosedir_suffix,
		struct env_t *private_ctx);
void gitfs_subcmd_log();
void gitfs_subcmd_diff();

#define ROOTENV ((struct env_t *) fuse_get_context()->private_data)

#endif
