#ifndef DELTA_H_
#define DELTA_H_

#include "common.h"

#define HASH_LIMIT 64
#define RABIN_SHIFT 23
#define RABIN_WINDOW 16
#define DELTA_SIZE_MIN	4

/*
 * The maximum size for any opcode sequence, including the initial header
 * plus Rabin window plus biggest copy.
 */
#define MAX_OP_SIZE	(5 + 5 + 1 + RABIN_WINDOW + 7)

struct index_entry {
	const unsigned char *ptr;
	unsigned int val;
};

struct unpacked_index_entry {
	struct index_entry entry;
	struct unpacked_index_entry *next;
};

struct delta_index {
	unsigned long memsize;
	const void *src_buf;
	unsigned long src_size;
	unsigned int hash_mask;
	struct index_entry *hash[0];
};

void *diff_delta(const void *src_buf, unsigned long src_bufsize,
		const void *trg_buf, unsigned long trg_bufsize,
		unsigned long *delta_size, unsigned long max_delta_size);

void *patch_delta(const void *src_buf, unsigned long src_size,
		const void *delta_buf, unsigned long delta_size,
		unsigned long *dst_size);

#endif
