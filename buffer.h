#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdio.h>
#include <unistd.h>

#define CHUNK 16384

off_t buffer_skip_bytes(FILE *src, off_t size);
off_t buffer_copy_bytes(FILE *src, FILE *dst, size_t size);

#endif
