#include "pack.h"

#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>

int read_pack_header(FILE *src, struct pack_header *hdr)
{
	size_t size = sizeof(*hdr);
	size_t in;
	in = fread(hdr, size, 1, src);
	if ((in < size && feof(src)) || ferror(src))
		die("Damaged pack hdr");
	else if (hdr->hdr_signature != htonl(PACK_SIGNATURE))
		die("Pack hdr is missing signature");
	if (ntohl(hdr->hdr_version) != PACK_VERSION)
		die("Pack version %d unsupported", ntohl(hdr->hdr_version));
	return 0;
}

int map_pack_idx(FILE *src)
{
	int srcfd;
	void *idx_map;
	size_t idx_size;
	uint32_t n, nr, i, *index;
	struct stat st;
	struct pack_idx_header *hdr;

	if ((srcfd = fileno(src)) < 0)
		return -errno;
	fstat(srcfd, &st);
	idx_size = st.st_size;
	if (idx_size < 4 * 256 + 20 + 20) {
		close(srcfd);
		die("Pack index too small");
	}
	idx_map = mmap(NULL, idx_size, PROT_READ, MAP_PRIVATE, srcfd, 0);
	close(srcfd);
	hdr = idx_map;
	if (hdr->idx_signature != htonl(PACK_IDX_SIGNATURE)) {
		munmap(idx_map, idx_size);
		die("Pack index missing v2 signature", ntohl(hdr->idx_version));
	}
	else if (ntohl(hdr->idx_version) != 2) {
		munmap(idx_map, idx_size);
		die("Pack index version %d unsupported", ntohl(hdr->idx_version));
	}
	index = (uint32_t *) hdr + 2;
	for (i = 0, nr = 0; i < 256; i++) {
		n = ntohl(index[i]);
		if (n < nr) {
			munmap(idx_map, idx_size);
			die("Non-monotonic index");
		}
		nr = n;
	}
	/*
	 * Minimum size:
	 *  - 8 bytes of header
	 *  - 256 index entries 4 bytes each
	 *  - 20-byte sha1 entry * nr
	 *  - 4-byte crc entry * nr
	 *  - 4-byte offset entry * nr
	 *  - 20-byte SHA1 of the packfile
	 *  - 20-byte SHA1 file checksum
	 * And after the 4-byte offset table might be a
	 * variable sized table containing 8-byte entries
	 * for offsets larger than 2^31.
	 */
	unsigned long min_size = 8 + 4*256 + nr*(20 + 4 + 4) + 20 + 20;
	unsigned long max_size = min_size;
	if (nr)
		max_size += (nr - 1) * 8;
	if (idx_size < min_size || idx_size > max_size) {
		munmap(idx_map, idx_size);
		die("wrong packfile index file size");
	}
	if (idx_size != min_size && (sizeof(off_t) <= 4)) {
		munmap(idx_map, idx_size);
		die("Pack too large for current definition of off_t");
	}
	munmap(idx_map, idx_size);
	return 0;
}
