all:: gitfs$X

CC = gcc
RM = rm -f
MV = mv

XDIFF_LIB=xdiff/lib.a
SHA1_LIB=block-sha1/lib.a

BUILTIN_OBJS =
BUILTIN_OBJS += common.o
BUILTIN_OBJS += main.o
BUILTIN_OBJS += fuse.o
BUILTIN_OBJS += buffer.o
BUILTIN_OBJS += compress.o
BUILTIN_OBJS += pack.o

CFLAGS = -g -O2 -Wall $(shell pkg-config fuse --cflags) $(shell pkg-config zlib --cflags)
LDFLAGS = $(shell pkg-config fuse --libs) $(shell pkg-config zlib --libs)
ALL_CFLAGS = $(CFLAGS)
ALL_LDFLAGS = $(LDFLAGS)
LIBS =

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

XDIFF_OBJS = xdiff/xdiffi.o xdiff/xprepare.o xdiff/xutils.o xdiff/xemit.o \
	xdiff/xmerge.o xdiff/xpatience.o

SHA1_OBJS = block-sha1/sha1.o

gitfs$X: $(BUILTIN_OBJS)
	$(QUIET_LINK)$(CC) $(ALL_CFLAGS) -o $@ $(BUILTIN_OBJS) \
		$(ALL_LDFLAGS) $(LIBS)

common.o: common.c gitfs.h
	$(QUIET_CC)$(CC) -o $*.o -c $(ALL_CFLAGS) $<

main.o: main.c gitfs.h
	$(QUIET_CC)$(CC) -o $*.o -c $(ALL_CFLAGS) $<

fuse.o: fuse.c gitfs.h
	$(QUIET_CC)$(CC) -o $*.o -c $(ALL_CFLAGS) $<

buffer.o: buffer.c buffer.h
	$(QUIET_CC)$(CC) -o $*.o -c $(ALL_CFLAGS) $<

compress.o: compress.c compress.h buffer.h
	$(QUIET_CC)$(CC) -o $*.o -c $(ALL_CFLAGS) $<

pack.o: pack.c pack.h
	$(QUIET_CC)$(CC) -o $*.o -c $(ALL_CFLAGS) $<

xdiff-interface.o $(XDIFF_OBJS): \
	xdiff/xinclude.h xdiff/xmacros.h xdiff/xdiff.h xdiff/xtypes.h \
	xdiff/xutils.h xdiff/xprepare.h xdiff/xdiffi.h xdiff/xemit.h

$(XDIFF_LIB): $(XDIFF_OBJS)
	$(QUIET_AR)$(RM) $@ && $(AR) rcs $@ $(XDIFF_OBJS)

$(SHA1_LIB): $(SHA1_OBJS)
	$(QUIET_AR)$(RM) $@ && $(AR) rcs $@ $(SHA1_OBJS)

clean:
	$(RM) gitfs$X *~ *.o xdiff/*.o block-sha1/*.o $(LIBS) $<

.PHONY: all clean FORCE
