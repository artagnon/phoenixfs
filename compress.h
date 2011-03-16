#ifndef COMPRESS_H_
#define COMPRESS_H_

#include <stdio.h>
#include "common.h"
#include "buffer.h"

int zdeflate(FILE *source, FILE *dest, int level);
int zinflate(FILE *source, FILE *dest);

#endif
