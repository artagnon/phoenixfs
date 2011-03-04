#include "buffer.h"
#include "compress.h"

#include <zlib.h>
#include <stdio.h>

int zdeflate(FILE *source, FILE *dest, int level)
{
	int ret, flush;
	unsigned int have;
	z_stream stream;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	if ((ret = deflateInit(&stream, level)) != Z_OK)
		return ret;

	do {
		stream.avail_in = fread(in, 1, CHUNK, source);
		if (ferror(source)) {
			deflateEnd(&stream);
			return Z_ERRNO;
		}
		flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
		stream.next_in = in;
		do {
			stream.avail_out = CHUNK;
			stream.next_out = out;
			ret = deflate(&stream, flush);
			have = CHUNK - stream.avail_out;
			if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
				deflateEnd(&stream);
				return Z_ERRNO;
			}
		} while (!stream.avail_out);
	} while (flush != Z_FINISH);

	deflateEnd(&stream);
	return Z_OK;
}

int zinflate(FILE *source, FILE *dest)
{
	int ret;
	unsigned int have;
	z_stream stream;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = 0;
	stream.next_in = Z_NULL;
	if ((ret = inflateInit(&stream)) != Z_OK)
		return ret;

	do {
		stream.avail_in = fread(in, 1, CHUNK, source);
		if (ferror(source)) {
			inflateEnd(&stream);
			return Z_ERRNO;
		}
		if (!stream.avail_in)
			break;
		stream.next_in = in;
		do {
			stream.avail_out = CHUNK;
			stream.next_out = out;
			ret = inflate(&stream, Z_NO_FLUSH);
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				inflateEnd(&stream);
				return ret;
			}
			have = CHUNK - stream.avail_out;
			if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
				inflateEnd(&stream);
				return Z_ERRNO;
			}
		} while (!stream.avail_out);
	} while (ret != Z_STREAM_END);

	inflateEnd(&stream);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}
