#ifndef PACK_H_
#define PACK_H_

#define PACK_SIGNATURE 0x5041434b
#define PACK_IDX_SIGNATURE 0xff744f63
#define PACK_VERSION 2

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "common.h"

struct pack_header {
	uint32_t hdr_signature;
	uint32_t hdr_version;
	uint32_t hdr_entries;
};

struct pack_idx_header {
	uint32_t idx_signature;
	uint32_t idx_version;
};

struct pack_idx_entry {
	unsigned char sha1[20];
	uint32_t crc32;
	off_t offset;
};

int read_pack_header(FILE *src, struct pack_header *hdr);
int map_pack_idx(FILE *src);

#endif
