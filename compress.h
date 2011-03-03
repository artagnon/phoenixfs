#ifndef COMPRESS_H_
#define COMPRESS_H_

#define CHUNK 16384

int zdeflate(FILE *source, FILE *dest, int level);
int zinflate(FILE *source, FILE *dest);

#endif
