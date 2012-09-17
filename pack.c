#include "pack.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>

static struct packed_git packroot;
static bool loaded_pack = false;

/* ---- Pack Lookup ---- */
/**
 * Note:
 * The CRC checksum table has been omitted
 */
static off_t nth_packed_object_offset(uint32_t n)
{
	const unsigned char *index = packroot.idx_data;
	uint32_t off;

	index += 4 * 256;
	index += 8 + packroot.nr * 20;
	off = ntohl(*((uint32_t *)(index + 4 * n)));
	if (!(off & 0x80000000))
		return off;
	index += packroot.nr * 4 + (off & 0x7fffffff) * 8;
	return (((uint64_t)ntohl(*((uint32_t *)(index + 0)))) << 32) |
		ntohl(*((uint32_t *)(index + 4)));
}

static int sha1_entry_pos(const void *table,
			size_t elem_size,
			size_t key_offset,
			unsigned lo, unsigned hi, unsigned nr,
			const unsigned char *key)
{
	const unsigned char *base = table;
	const unsigned char *hi_key, *lo_key;
	unsigned ofs_0;

	if (!nr || lo >= hi)
		return -1;

	if (nr == hi)
		hi_key = NULL;
	else
		hi_key = base + elem_size * hi + key_offset;
	lo_key = base + elem_size * lo + key_offset;

	ofs_0 = 0;
	do {
		int cmp;
		unsigned ofs, mi, range;
		unsigned lov, hiv, kyv;
		const unsigned char *mi_key;

		range = hi - lo;
		if (hi_key) {
			for (ofs = ofs_0; ofs < 20; ofs++)
				if (lo_key[ofs] != hi_key[ofs])
					break;
			ofs_0 = ofs;
			/*
			 * byte 0 thru (ofs-1) are the same between
			 * lo and hi; ofs is the first byte that is
			 * different.
			 */
			hiv = hi_key[ofs_0];
			if (ofs_0 < 19)
				hiv = (hiv << 8) | hi_key[ofs_0+1];
		} else {
			hiv = 256;
			if (ofs_0 < 19)
				hiv <<= 8;
		}
		lov = lo_key[ofs_0];
		kyv = key[ofs_0];
		if (ofs_0 < 19) {
			lov = (lov << 8) | lo_key[ofs_0+1];
			kyv = (kyv << 8) | key[ofs_0+1];
		}
		assert(lov < hiv);

		if (kyv < lov)
			return -1 - lo;
		if (hiv < kyv)
			return -1 - hi;

		/*
		 * Even if we know the target is much closer to 'hi'
		 * than 'lo', if we pick too precisely and overshoot
		 * (e.g. when we know 'mi' is closer to 'hi' than to
		 * 'lo', pick 'mi' that is higher than the target), we
		 * end up narrowing the search space by a smaller
		 * amount (i.e. the distance between 'mi' and 'hi')
		 * than what we would have (i.e. about half of 'lo'
		 * and 'hi').  Hedge our bets to pick 'mi' less
		 * aggressively, i.e. make 'mi' a bit closer to the
		 * middle than we would otherwise pick.
		 */
		kyv = (kyv * 6 + lov + hiv) / 8;
		if (lov < hiv - 1) {
			if (kyv == lov)
				kyv++;
			else if (kyv == hiv)
				kyv--;
		}
		mi = (range - 1) * (kyv - lov) / (hiv - lov) + lo;

		if (!(lo <= mi && mi < hi))
			die("assertion failure lo %u mi %u hi %u",
				lo, mi, hi);

		mi_key = base + elem_size * mi + key_offset;
		cmp = memcmp(mi_key + ofs_0, key + ofs_0, 20 - ofs_0);
		if (!cmp)
			return mi;
		if (cmp > 0) {
			hi = mi;
			hi_key = mi_key;
		} else {
			lo = mi + 1;
			lo_key = mi_key + elem_size;
		}
	} while (lo < hi);
	return -lo-1;
}

off_t find_pack_entry(const unsigned char *sha1)
{
	const uint32_t *fanout = packroot.idx_data;
	const unsigned char *sha1_idx = packroot.idx_data;
	unsigned hi, lo, stride;
	char sha1_digest[40];
	int pos;

	/* Skip the header and go  to the SHA1 section */
	fanout += 2;
	sha1_idx += 8;
	sha1_idx += 4 * 256;

	hi = ntohl(fanout[*sha1]);
	lo = ((*sha1 == 0x0) ? 0 : ntohl(fanout[*sha1 - 1]));
	stride = 20;

	print_sha1(sha1_digest, sha1);
	PHOENIXFS_DBG("find_pack_entry:: %s %u %u %u", sha1_digest,
		lo, hi, packroot.nr);

	pos = sha1_entry_pos(sha1_idx, stride, 0,
			lo, hi, packroot.nr, sha1);

	PHOENIXFS_DBG("find_pack_entry:: pos = %d", pos);

	if (pos < 0)
		return 0;
	return nth_packed_object_offset(pos);
}

/**
 * Format:
 * <> = [sha1 | delta_bit | size | data]
 */
int unpack_entry(unsigned char *sha1, const char *loosedir)
{
	unsigned char read_sha1[20];
	off_t obj_offset, size;
	char sha1_digest[40];
	char xpath[PATH_MAX];
	FILE *loosefh;
	bool delta;

	print_sha1(sha1_digest, sha1);

	/* Convert SHA1 to offset and make sure we were successful */
	if (!(obj_offset = find_pack_entry(sha1))) {
		PHOENIXFS_DBG("unpack_entry:: missing %s", sha1_digest);
		return -1;
	}
	PHOENIXFS_DBG("unpack_entry:: %s %lld", sha1_digest,
		(long long int)obj_offset);
	if (fseek(packroot.packfh, obj_offset, SEEK_SET) < 0)
		die("Seek error: packroot.packfh");
	if (fread(&read_sha1, 20 * sizeof(unsigned char), 1, packroot.packfh) < 1)
		die("Read error: read_sha1");
	assert(memcmp(sha1, read_sha1, 20) == 0);

	if (fread(&delta, sizeof(bool), 1, packroot.packfh) < 1)
		die("Read error: delta");
	sprintf(xpath, "%s/%s", loosedir, sha1_digest);
	if (!(loosefh = fopen(xpath, "wb+"))) {
		PHOENIXFS_DBG("unpack_entry:: can't open %s", xpath);
		return -errno;
	}
	if (!delta) {
		if (fread(&size, sizeof(off_t), 1, packroot.packfh) < 1)
			die("Read error: %lld", (long long int)size);
		PHOENIXFS_DBG("unpack_entry:: non-delta %s", sha1_digest);
		buffer_copy_bytes(packroot.packfh, loosefh, size);
	}
	else
		PHOENIXFS_DBG("unpack_entry:: delta %s", sha1_digest);
	fclose(loosefh);
	return 0;
}

/* ---- Pack Read ---- */
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
	if (idx_size < 4 * 256) {
		close(srcfd);
		die("Pack index too small");
	}
	idx_map = mmap(NULL, idx_size, PROT_READ, MAP_PRIVATE, srcfd, 0);
	close(srcfd);
	fclose(src);
	hdr = idx_map;
	if (hdr->signature != htonl(PACK_IDX_SIGNATURE)) {
		munmap(idx_map, idx_size);
		die("Corrupt pack index signature");
	}
	else if (hdr->version != htonl(PACK_IDX_VERSION)) {
		munmap(idx_map, idx_size);
		die("Wrong pack index version");
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
	 *  - 4-byte offset entry * nr
	 *
	 * Omitted fields:
	 *  - 4-byte crc entry * nr
	 *  - 20-byte SHA1 of the packfile
	 *  - 20-byte SHA1 file checksum
	 */
	unsigned long min_size = 8 + 4*256 + nr*(20 + 4);
	unsigned long max_size = min_size;
	if (nr)
		max_size += (nr - 1) * 8;
	if (idx_size < min_size || idx_size > max_size) {
		munmap(idx_map, idx_size);
		die("Wrong packfile index file size");
	}
	if (idx_size != min_size && (sizeof(off_t) <= 4)) {
		munmap(idx_map, idx_size);
		die("Pack too large for current definition of off_t");
	}
	packroot.idx_data = idx_map;
	packroot.idx_size = idx_size;
	packroot.nr = nr;
	return 0;
}

static int load_pack_idx(const char *path)
{
	FILE *idx_file;

	if (packroot.idx_data)
		return -1;
	if (!(idx_file = fopen(path, "rb")))
		return -errno;
	return map_pack_idx(idx_file);
}

int load_packing_info(const char *pack_path, const char *idx_path,
		bool existing_pack)
{
	struct stat st;
	struct pack_header hdr;

	PHOENIXFS_DBG("load_packing_info:: %s %s %d", pack_path, idx_path, existing_pack);
	if (!(loaded_pack = existing_pack)) {
		/* Nothing to load */
		strcpy(packroot.pack_path, pack_path);
		strcpy(packroot.idx_path, idx_path);
		return 0;
	}
	if (load_pack_idx(idx_path) < 0)
		die("Packfile index %s missing", idx_path);

	if (!(packroot.packfh = fopen(pack_path, "ab+")) ||
		(stat(pack_path, &st) < 0))
		die("Can't open pack %s", pack_path);

	packroot.pack_size = st.st_size;
	strcpy(packroot.pack_path, pack_path);
	strcpy(packroot.idx_path, idx_path);

	/*
	 * Minimum size:
	 *  - 8 bytes of header
	 *
	 * Omitted fields:
	 *  - 4 bytes of entries
	 *  - 20-byte packfile SHA1 checksum
	 */
	if (packroot.pack_size < 8)
		die("Wrong packfile file size");

	/* Verify we recognize this pack file format */
	rewind(packroot.packfh);
	if (fread(&hdr, sizeof(hdr), 1, packroot.packfh) < 1)
		die("Read error: hdr");
	if (hdr.signature != htonl(PACK_SIGNATURE))
		die("Corrupt pack signature: %d", ntohl(hdr.signature));
	if (hdr.version != htonl(PACK_VERSION))
		die("Wrong pack version: %d", ntohl(hdr.version));
	fseek(packroot.packfh, 0L, SEEK_END);
	/* Omit entries */

	return 0;
}

/* ---- Pack Write ---- */
static int sha1_compare(const void *_a, const void *_b)
{
	struct pack_idx_entry *a = *(struct pack_idx_entry **)_a;
	struct pack_idx_entry *b = *(struct pack_idx_entry **)_b;
	return memcmp(a->sha1, b->sha1, 20);
}

void unmap_write_idx(struct pack_idx_entry *objects[], int nr_objects)
{
	struct pack_idx_entry **sorted_by_sha, **list, **last;
	unsigned int nr_large_offset;
	struct pack_idx_header hdr;
	off_t last_obj_offset = 0;
	char sha1_digest[40];
	uint32_t array[256];
	register int i;
	FILE *idxfh;

	if (nr_objects) {
		sorted_by_sha = objects;
		list = sorted_by_sha;
		last = sorted_by_sha + nr_objects;
		for (i = 0; i < nr_objects; ++i) {
			if (objects[i]->offset > last_obj_offset)
				last_obj_offset = objects[i]->offset;
		}
		qsort(sorted_by_sha, nr_objects, sizeof(sorted_by_sha[0]),
		      sha1_compare);
	}
	else
		sorted_by_sha = list = last = NULL;

	if (loaded_pack)
		munmap((void *) packroot.idx_data, packroot.idx_size);
	if (!(idxfh = fopen(packroot.idx_path, "wb+")))
		die("Can't create idx file: %s", packroot.idx_path);

	hdr.signature = htonl(PACK_IDX_SIGNATURE);
	hdr.version = htonl(PACK_IDX_VERSION);
	fwrite(&hdr, sizeof(hdr), 1, idxfh);

	/* Write the fanout table */
	for (i = 0; i < 256; i++) {
		struct pack_idx_entry **next = list;
		while (next < last) {
			struct pack_idx_entry *obj = *next;
			if (obj->sha1[0] != i)
				break;
			next++;
		}
		array[i] = htonl(next - sorted_by_sha);
		list = next;
	}
	fwrite(&array, 256 * sizeof(uint32_t), 1, idxfh);

	/* Write the actual SHA1 entries: 20 * nr */
	list = sorted_by_sha;
	for (i = 0; i < nr_objects; i++) {
		struct pack_idx_entry *obj = *list++;
		fwrite(obj->sha1, 20, 1, idxfh);
	}

	/* Omit the crc32 table: 4 * nr */
	/* Write the 32-bit offset table: 4 * nr */
	nr_large_offset = 0;
	list = sorted_by_sha;
	for (i = 0; i < nr_objects; i++) {
		struct pack_idx_entry *obj = *list++;
		uint32_t offset = (obj->offset <= pack_idx_off32_limit) ?
			obj->offset : (0x80000000 | nr_large_offset++);
		offset = htonl(offset);
		print_sha1(sha1_digest, obj->sha1);
		PHOENIXFS_DBG("unmap_write_idx:: %s %llu", sha1_digest,
			(long long int)obj->offset);
		fwrite(&offset, sizeof(uint32_t), 1, idxfh);
	}

	/* Write the 64-bit offset table: 8 * nr */
	list = sorted_by_sha;
	while (nr_large_offset) {
		struct pack_idx_entry *obj = *list++;
		uint64_t offset = obj->offset;
		if (offset > pack_idx_off32_limit) {
			uint32_t split[2];
			split[0] = htonl(offset >> 32);
			split[1] = htonl(offset & 0xffffffff);
			fwrite(split, sizeof(uint64_t), 1, idxfh);
			nr_large_offset--;
		}
	}
	/* Omit the checksum trailer: 2 * 20 */
	fclose(idxfh);
}

static int write_pack_hdr(const char *pack_path)
{
	FILE *packfh;
	struct stat st;
	struct pack_header hdr;

	if (!(packfh = fopen(pack_path, "wb+")) ||
		(stat(pack_path, &st) < 0))
		return -errno;
	packroot.packfh = packfh;
	packroot.pack_size = st.st_size;

	hdr.signature = htonl(PACK_SIGNATURE);
	hdr.version = htonl(PACK_VERSION);
	fwrite(&hdr, sizeof(hdr), 1, packfh);
	return 0;
}

void dump_packing_info(const char *loosedir)
{
	PHOENIXFS_DBG("dump_packing_info:: %s", loaded_pack ? "append" : "create");
	if (!loaded_pack)
		write_pack_hdr(packroot.pack_path);
	packup_loose_objects(packroot.packfh, packroot.idx_data,
			packroot.nr, loosedir);
	fclose(packroot.packfh);
}

void mark_for_packing(const unsigned char *sha1, size_t size)
{
	add_loose_entry((unsigned char *)sha1, size);
}
