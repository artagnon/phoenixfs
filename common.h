#ifndef COMMON_H_
#define COMMON_H_

#if 1
#define GITFS_DBG(f, ...) \
{ \
	FILE *logfh = fopen("/tmp/gitfs.log", "a");	\
	if (logfh) { \
		fprintf(logfh, "l. %4d: " f "\n", __LINE__, ##__VA_ARGS__); \
		fclose(logfh); \
	} \
}
#else
#define GITFS_DBG(f, ...) while(0)
#endif

void die(const char *err, ...);

#endif
