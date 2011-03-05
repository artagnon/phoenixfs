#include "gitfs.h"
#include "buffer.h"
#include "sha1.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int sha1_file(FILE *infile, size_t size, unsigned char *sha1)
{
	size_t done, chunk, in;
	SHA1_CTX c;
	void *buf;

	SHA1_Init(&c);
	if (!(buf = malloc(CHUNK)))
		return -errno;
	done = 0;
	do {
		chunk = (CHUNK < (size - done) ? CHUNK : (size - done));
		in = fread(buf, 1, chunk, infile);
		SHA1_Update(&c, buf, in);
		done += in;
	} while(done < size && !ferror(infile));
	free(buf);
	if (ferror(infile) || in < size)
		die("Short SHA1 read");
	SHA1_Final(sha1, &c);
	return 0;
}
