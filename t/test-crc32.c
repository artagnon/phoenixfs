#include "crc32.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
	uint32_t crc = ~0;
	unsigned char *data;
	size_t length;

	if (argc < 2)
		die("Usage: %s <input string>", argv[0]);
	length = strlen(argv[1]);
	data = (unsigned char *) argv[1];
	printf("%08X\n", compute_crc32(crc, data, length));
	return 0;
}
