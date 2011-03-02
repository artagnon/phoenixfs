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
	uid_t uid, gid;
	time_t now;
};

enum subcmd {
	SUBCMD_NONE,
	SUBCMD_INIT,
	SUBCMD_LOG,
	SUBCMD_DIFF,
};

void die(const char *err, ...);
int gitfs_fuse(int argc, char **argv);
void gitfs_subcmd_init(const char *mountpoint, const char *datapath);
void gitfs_subcmd_log();
void gitfs_subcmd_diff();

#endif
