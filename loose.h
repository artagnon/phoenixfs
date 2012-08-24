#ifndef LOOSE_H_
#define LOOSE_H_

#include "common.h"
#include "buffer.h"
#include "sha1.h"

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

struct pack_idx_entry {
	unsigned char sha1[20];
	off_t offset;
	size_t size;
};

struct loose_buf {
	uint32_t nr;
	struct pack_idx_entry *entries[0]; /* GCC hack: 0 length */
};

void add_loose_entry(const unsigned char *sha1, size_t size);
void packup_loose_objects(FILE *packfh, const void *idx_data,
			uint32_t idx_nr, const char *loosedir);
void unmap_write_idx(struct pack_idx_entry *objects[], int nr_objects);
off_t find_pack_entry(const unsigned char *sha1);

#endif
