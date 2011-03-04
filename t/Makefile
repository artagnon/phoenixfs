all:: test-diff$X

CC = gcc
RM = rm -f
MV = mv

XDIFF_LIB=../xdiff/lib.a

BUILD_OBJS =
BUILD_OBJS += test-diff.o

CFLAGS = -g -O2 -Wall $(shell pkg-config fuse --cflags) $(shell pkg-config zlib --cflags)
LDFLAGS = $(shell pkg-config fuse --libs) $(shell pkg-config zlib --libs)
ALL_CFLAGS = $(CFLAGS)
ALL_LDFLAGS = $(LDFLAGS)
LIBS = $(XDIFF_LIB)

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

test-diff$X: test-diff.o ../diff.o ../common.o
	$(QUIET_LINK)$(CC) -I../ $(ALL_CFLAGS) -o $@ test-diff.o ../diff.o ../common.o \
		$(ALL_LDFLAGS) $(LIBS)

test-diff.o: test-diff.c ../diff.c
	$(QUIET_CC)$(CC) -I../ -o $*.o -c $(ALL_CFLAGS) $<

clean:
	$(RM) test-diff$X *~ *.o $(LIBS) $<

.PHONY: all clean FORCE
