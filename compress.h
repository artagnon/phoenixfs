#ifndef COMPRESS_H_
#define COMPRESS_H_

#include <stdio.h>

int zdeflate(FILE *source, FILE *dest, int level);
int zinflate(FILE *source, FILE *dest);

#endif
