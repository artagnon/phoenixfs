#ifndef SHA1_H_
#define SHA1_H_

#include <stdio.h>
#include "common.h"
#include "block-sha1/sha1.h"

#define SHA1_CTX	blk_SHA_CTX
#define SHA1_Init	blk_SHA1_Init
#define SHA1_Update	blk_SHA1_Update
#define SHA1_Final	blk_SHA1_Final

int sha1_file(FILE *infile, size_t size, unsigned char *sha1);
void print_sha1(char *dst, const unsigned char *sha1);
int get_sha1_hex(const char *hex, unsigned char *sha1);

#endif
