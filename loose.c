#include "loose.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

struct loose_buf looseroot = {0};
static char xpath[PATH_MAX];

void add_loose_entry(const unsigned char *sha1, size_t size)
{
	uint32_t this_nr;
	char sha1_digest[40];

	this_nr = looseroot.nr;
	print_sha1(sha1_digest, sha1);
	PHOENIXFS_DBG("add_loose_entry:: %s [%d]", sha1_digest, this_nr);
	looseroot.entries[this_nr] = malloc(sizeof(struct pack_idx_entry));
	memcpy(looseroot.entries[this_nr]->sha1, sha1, 20);
	looseroot.entries[this_nr]->size = size;
	looseroot.nr ++;
}

/**
 * Format:
 * <> = [sha1 | delta_bit | size | data]
 */
void packup_loose_objects(FILE *packfh, const void *idx_data,
			uint32_t idx_nr, const char *loosedir)
{
	register int i;
	FILE *datafh;
	struct stat st;
	bool delta = false;
	char sha1_digest[40];
	struct pack_idx_entry *this_entry;

	/* For parsing out existing data in idx_map */
	void *sha1_offset;
	unsigned char this_sha1[20];
	off_t this_offset;
	int existing_nr;

	fseek(packfh, 0L, SEEK_END);
	for (i = 0; i < looseroot.nr; i++) {
		this_entry = looseroot.entries[i];
		/* Set the offset for writing packfile index */
		this_entry->offset = ftell(packfh);
		/* Write the SHA1 */
		fwrite(this_entry->sha1, 20, 1, packfh);
		/* Write delta bit */
		/* TODO: Generate real deltas! */
		fwrite(&delta, sizeof(bool), 1, packfh);

		/* Write the zlib stream or delta */
		print_sha1(sha1_digest, this_entry->sha1);
		sprintf(xpath, "%s/%s", loosedir, sha1_digest);
		if ((lstat(xpath, &st) < 0) ||
			!(datafh = fopen(xpath, "rb"))) {
			PHOENIXFS_DBG("packup_loose_objects:: Creating %s", sha1_digest);
			datafh = fopen(xpath, "wb");
		}
		PHOENIXFS_DBG("packup_loose_objects:: %s [%d] %lld", sha1_digest,
			i, (long long int)this_entry->offset);
		fwrite(&(st.st_size), sizeof(off_t), 1, packfh);
		buffer_copy_bytes(datafh, packfh, st.st_size);
		fclose(datafh);
	}

	/* If there is no pre-existing idx, just write the new idx */
	if (!idx_data) {
		unmap_write_idx(looseroot.entries, looseroot.nr);
		return;
	}

	/* Update index */
	/* First, add entries from the existing idx_data */
	/* Skip header and fanout table */
	sha1_offset = (void *) (idx_data + 8 + 256 * 4);
	existing_nr = looseroot.nr;
	for (i = 0; i < idx_nr; i++) {
		memcpy(&this_sha1, sha1_offset, 20);
		print_sha1(sha1_digest, this_sha1);
		add_loose_entry(this_sha1, 0);
		if (!(this_offset = find_pack_entry(this_sha1)))
			PHOENIXFS_DBG("packup_loose_objects:: "
				"update idx missing %s", sha1_digest);
		looseroot.entries[i + existing_nr]->offset = this_offset;
		sha1_offset += 20 * sizeof(unsigned char);
	}

	/* Write the new entries */

	/* Finally, rewrite the idx */
	unmap_write_idx(looseroot.entries, looseroot.nr);
}
