#ifndef GITFS_H_
#define GITFS_H_

#define FUSE_USE_VERSION 26
#define _XOPEN_SOURCE 500

#include <unistd.h>
#include <time.h>
#include <stdio.h>

struct env_t {
	const char *path;
	uid_t uid, gid;
	time_t now;
};

struct gitfs_state {
    FILE *logfile;
    char *rootdir;
};

#define GITFS_DATA ((struct gitfs_state *) fuse_get_context()->private_data)

void die(const char *err, ...);
int gitfs_fuse(int argc, char **argv);

#endif
