#ifndef SHA1_H_
#define SHA1_H_

#include "block-sha1/sha1.h"

#define SHA1_CTX	blk_SHA_CTX
#define SHA1_Init	blk_SHA1_Init
#define SHA1_Update	blk_SHA1_Update
#define SHA1_Final	blk_SHA1_Final

int sha1_file(FILE *infile, unsigned long size, unsigned char *sha1);
void print_sha1(char *dst, unsigned char *sha1);

#endif
