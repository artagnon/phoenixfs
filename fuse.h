#ifndef FUSE_H_
#define FUSE_H_

#define FUSE_USE_VERSION 26
#define _XOPEN_SOURCE 500
#define GITFS_MAGIC 0x2888
#define DT_DIR 4

#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <fuse.h>

#include "common.h"
#include "sha1.h"
#include "buffer.h"
#include "btree.h"
#include "fstree.h"
#include "compress.h"
#include "pack.h"

int gitfs_fuse(int argc, char *argv[]);

#define ROOTENV ((struct env_t *) fuse_get_context()->private_data)

#endif
