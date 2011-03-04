#include "gitfs.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static void vreportf(const char *fmt, va_list params)
{
	char msg[4096];
	vsnprintf(msg, sizeof(msg), fmt, params);
	fprintf(stderr, "%s\n", msg);
}

void die(const char *fmt, ...)
{
	va_list params;
	va_start(params, fmt);
	vreportf(fmt, params);
	va_end(params);
	exit(128);
}
