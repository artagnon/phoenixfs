#include "buffer.h"

#include <stdio.h>
#include <unistd.h>

static unsigned char buf[CHUNK];

off_t buffer_skip_bytes(FILE *src, off_t size)
{
	size_t in;
	off_t done = 0;
	while (done < size && !feof(src) && !ferror(src)) {
		in = (size - done) < CHUNK ? (size - done) : CHUNK;
		done += fread(buf, 1, in, src);
	}
	return done;
}

off_t buffer_copy_bytes(FILE *src, FILE *dst, size_t size)
{
	size_t in;
	off_t done = 0;
	while (done < size && !feof(src) && !ferror(src)) {
		in = (size - done) < CHUNK ? (size - done) : CHUNK;
		in = fread(buf, 1, in, src);
		if (ferror(src) || feof(src))
			return done + in;
		done += in;
		fwrite(buf, 1, in, dst);
		if (ferror(dst) || feof(dst))
			return done + buffer_skip_bytes(src, size - done);
	}
	return done;
}
