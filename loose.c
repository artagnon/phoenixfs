#include "loose.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

struct loose_buf looseroot = {0};
static char xpath[PATH_MAX];

static int size_compare(const void *_a, const void *_b)
{
	const struct pack_idx_entry *a = (struct pack_idx_entry *)_a;
	const struct pack_idx_entry *b = (struct pack_idx_entry *)_b;
	return (a->size < b->size) ? -1 : (a->size > b->size);
}

void add_loose_entry(unsigned char *sha1, size_t size)
{
	uint32_t this_nr;
	char sha1_digest[40];

	this_nr = looseroot.nr;
	print_sha1(sha1_digest, sha1);
	GITFS_DBG("add_loose_entry:: %s [%d]", sha1_digest, this_nr);
	looseroot.entries[this_nr] = malloc(sizeof(struct loose_buf));
	memcpy(looseroot.entries[this_nr]->sha1, sha1, 20);
	looseroot.entries[this_nr]->size = size;
	looseroot.nr ++;
}

void packup_loose_objects(FILE *packfh, const char *loosedir)
{
	register int i;
	FILE *datafh;
	struct stat st;
	bool delta = false;
	char sha1_digest[40];
	struct pack_idx_entry *this_entry;

	/* Sort by size and prepare deltas before packing */
	qsort(&looseroot, looseroot.nr,
		sizeof(struct pack_idx_entry), size_compare);
	for (i = 0; i < looseroot.nr; i++) {
		this_entry = looseroot.entries[i];
		/* Set the offset for writing packfile index */
		this_entry->offset = ftell(packfh);
		/* Write the SHA1 */
		fwrite(this_entry->sha1, 20 * sizeof(unsigned char), 1, packfh);
		/* Write delta bit */
		/* TODO: Generate real deltas! */
		fwrite(&delta, sizeof(bool), 1, packfh);

		/* Write the zlib stream or delta */
		print_sha1(sha1_digest, this_entry->sha1);
		sprintf(xpath, "%s/%s", loosedir, sha1_digest);
		GITFS_DBG("packup_loose_objects:: %s [%d]", sha1_digest, i);
		if (!(datafh = fopen(xpath, "rb")) ||
			(lstat(xpath, &st) < 0))
			buffer_copy_bytes(datafh, packfh, st.st_size);
		fclose(datafh);
	}
	unmap_write_idx(looseroot.entries, looseroot.nr);
}
