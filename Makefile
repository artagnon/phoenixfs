all:: gitfs$X

CC = gcc
RM = rm -f
MV = mv

CFLAGS = -g -O2 -Wall $(shell pkg-config fuse --cflags)
LDFLAGS = $(shell pkg-config fuse --libs)
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

gitfs$X: main.o fuse.o
	$(QUIET_LINK)$(CC) $(ALL_CFLAGS) -o $@ main.o fuse.o \
		$(ALL_LDFLAGS) $(LIBS)

main.o: main.c gitfs.h
	$(QUIET_CC)$(CC) -o $*.o -c $(ALL_CFLAGS) $<

fuse.o: fuse.c gitfs.h
	$(QUIET_CC)$(CC) -o $*.o -c $(ALL_CFLAGS) $<

clean:
	$(RM) gitfs$X gitfs.o fuse.o *~ $<

.PHONY: all clean FORCE
