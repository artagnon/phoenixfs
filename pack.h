#ifndef PACK_H_
#define PACK_H_

#define PACK_SIGNATURE 0x5041434b
#define PACK_IDX_SIGNATURE 0xff744f63
#define PACK_IDX_VERSION 2
#define PACK_VERSION 3
#define pack_idx_off32_limit 0x7fffffff

#include "common.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>

struct packed_git {
	char pack_path[PATH_MAX];
	char idx_path[PATH_MAX];
	FILE *packfh;
	const void *idx_data;
	size_t pack_size;
	size_t idx_size;
	uint32_t nr;
};

/* Omit number of entries in pack */
struct pack_header {
	uint32_t signature;
	uint32_t version;
};

struct pack_idx_header {
	uint32_t signature;
	uint32_t version;
};

/* pack_idx_entry contains a trailing size field that's used only by
   the loose module internally; we will strip it off before writing it
   to the actual index */
struct pack_idx_entry {
	unsigned char sha1[20];
	off_t offset;
	size_t size;
};

int initialize_pack_file(const char *pack_path, const char *idx_path);
int load_packing_info(const char *pack_path, const char *idx_path,
		bool existing_pack);
void dump_packing_info(const char *loosedir);
int map_pack_idx(FILE *src);
void unmap_write_idx(struct pack_idx_entry *objects[], int nr_objects);
void packup_loose_objects(FILE *packfh, const char *loosedir);
void mark_for_packing(unsigned char *sha1, size_t size);
void add_loose_entry(unsigned char *sha1, size_t size);

#endif
