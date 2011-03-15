CC = gcc
RM = rm -f
MV = mv

XDIFF_LIB=../xdiff/lib.a
SHA1_LIB=../block-sha1/lib.a

BUILD_OBJS =
BUILD_OBJS += test-pack.o
BUILD_OBJS += test-diff.o
BUILD_OBJS += test-sha1.o
BUILD_OBJS += test-btree.o
BUILD_OBJS += test-crc32.o

ALL_TARGETS =
ALL_TARGETS += test-pack
ALL_TARGETS += test-diff
ALL_TARGETS += test-sha1
ALL_TARGETS += test-btree
ALL_TARGETS += test-crc32

CFLAGS = -g -O2 -Wall $(shell pkg-config fuse --cflags) $(shell pkg-config zlib --cflags)
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
	QUIET_LINK    = @echo '   ' LINK $@;
	QUIET_SUBDIR0 = +@subdir=
	QUIET_SUBDIR1 = ;$(NO_SUBDIR) echo '   ' SUBDIR $$subdir; \
	                $(MAKE) $(PRINT_DIR) -C $$subdir
endif
endif

XDIFF_OBJS = ../xdiff/xdiffi.o ../xdiff/xprepare.o ../xdiff/xutils.o \
	../xdiff/xemit.o ../xdiff/xmerge.o ../xdiff/xpatience.o

SHA1_OBJS = ../block-sha1/sha1.o

all:: $(ALL_TARGETS)

test-%.o: test-%.c ../%.c ../common.c
	$(QUIET_CC)$(CC) -I../ -o test-$*.o -c $(ALL_CFLAGS) $<

test-pack$X: test-pack.o ../pack.o ../common.o
	$(QUIET_LINK)$(CC) -I../ $(ALL_CFLAGS) -o $@ test-pack.o \
		../pack.o ../common.o \
		$(ALL_LDFLAGS)

test-diff$X: test-diff.o ../diff.o ../common.o $(XDIFF_LIB)
	$(QUIET_LINK)$(CC) -I../ $(ALL_CFLAGS) -o $@ test-diff.o \
		../diff.o ../common.o \
		$(ALL_LDFLAGS) $(XDIFF_LIB)

test-sha1$X: test-sha1.o ../sha1.o ../common.o $(SHA1_LIB)
	$(QUIET_LINK)$(CC) -I../ $(ALL_CFLAGS) -o $@ test-sha1.o \
		../sha1.o ../common.o \
		$(ALL_LDFLAGS) $(SHA1_LIB)

test-btree$X: test-btree.o ../btree.o ../common.o
	$(QUIET_LINK)$(CC) -I../ $(ALL_CFLAGS) -o $@ test-btree.o \
		../btree.o ../common.o \
		$(ALL_LDFLAGS) $(SHA1_LIB)

test-crc32$X: test-crc32.o ../crc32.o ../common.o
	$(QUIET_LINK)$(CC) -I../ $(ALL_CFLAGS) -o $@ test-crc32.o \
		../crc32.o ../common.o \
		$(ALL_LDFLAGS) $(SHA1_LIB)

$(XDIFF_LIB): $(XDIFF_OBJS)
	$(QUIET_AR)$(RM) $@ && $(AR) rcs $@ $(XDIFF_OBJS)

$(SHA1_LIB): $(SHA1_OBJS)
	$(QUIET_AR)$(RM) $@ && $(AR) rcs $@ $(SHA1_OBJS)

clean:
	$(RM) $(ALL_TARGETS) $(BUILD_OBJS) $<

.PHONY: all clean FORCE
