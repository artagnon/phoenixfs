#ifndef CRC32_H_
#define CRC32_H_

#include "common.h"
#include <stdint.h>
#include <unistd.h>

uint32_t compute_crc32(uint32_t crc, const uint8_t *data, size_t length);

#endif
