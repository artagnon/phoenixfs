CC = gcc
RM = rm -f
MV = mv

XDIFF_LIB = xdiff/lib.a
SHA1_LIB = block-sha1/lib.a

BUILTIN_OBJS =
BUILTIN_OBJS += common.o
BUILTIN_OBJS += main.o
BUILTIN_OBJS += fuse.o
BUILTIN_OBJS += buffer.o
BUILTIN_OBJS += compress.o
BUILTIN_OBJS += pack.o
BUILTIN_OBJS += sha1.o
BUILTIN_OBJS += diff.o
BUILTIN_OBJS += btree.o
BUILTIN_OBJS += crc32.o
BUILTIN_OBJS += fstree.o
BUILTIN_OBJS += persist.o
BUILTIN_OBJS += delta.o
BUILTIN_OBJS += loose.o

ALL_TARGETS = phoenixfs

CFLAGS = -g -O0 -Wall -Werror $(shell pkg-config fuse --cflags) $(shell pkg-config zlib --cflags)
LDFLAGS = $(shell pkg-config fuse --libs) $(shell pkg-config zlib --libs)
ALL_CFLAGS = $(CFLAGS)
ALL_LDFLAGS = $(LDFLAGS)
ALL_LIBS = $(XDIFF_LIB) $(SHA1_LIB)

QUIET_SUBDIR0 = +$(MAKE) -C # space to separate -C and subdir
QUIET_SUBDIR1 =

ifneq ($(findstring $(MAKEFLAGS),w),w)
PRINT_DIR = --no-print-directory
else # "make -w"
NO_SUBDIR = :
endif

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
	QUIET_CC      = @echo '   ' CC $@;
	QUIET_AR      = @echo '   ' AR $@;
	QUIET_LINK    = @echo '   ' LINK $@;
	QUIET_SUBDIR0 = +@subdir=
	QUIET_SUBDIR1 = ;$(NO_SUBDIR) echo '   ' SUBDIR $$subdir; \
	                $(MAKE) $(PRINT_DIR) -C $$subdir
endif
endif

XDIFF_OBJS = xdiff/xdiffi.o xdiff/xprepare.o xdiff/xutils.o xdiff/xemit.o \
	xdiff/xmerge.o xdiff/xpatience.o

SHA1_OBJS = block-sha1/sha1.o

all:: $(ALL_TARGETS)

phoenixfs$X: $(BUILTIN_OBJS) $(ALL_LIBS)
	$(QUIET_LINK)$(CC) $(ALL_CFLAGS) -o $@ $(BUILTIN_OBJS) \
		$(ALL_LDFLAGS) $(ALL_LIBS)

%.o: %.c %.h btree.h
	$(QUIET_CC)$(CC) -o $*.o -c $(ALL_CFLAGS) $<

$(XDIFF_LIB): $(XDIFF_OBJS)
	$(QUIET_AR)$(RM) $@ && $(AR) rcs $@ $^

$(SHA1_LIB): $(SHA1_OBJS)
	$(QUIET_AR)$(RM) $@ && $(AR) rcs $@ $^

test:
	$(MAKE) -C t

clean:
	$(RM) $(ALL_TARGETS) $(BUILTIN_OBJS) $(XDIFF_OBJS) \
	$(SHA1_OBJS) $(ALL_LIBS)

.PHONY: all clean FORCE
