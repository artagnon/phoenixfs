#!/bin/sh

# Reset environment
LANG=C
LC_ALL=C
PAGER=cat
TZ=UTC
TERM=dumb
export LANG LC_ALL PAGER TERM TZ
EDITOR=:
unset VISUAL
unset EMAIL
GIT_MERGE_VERBOSITY=5
export EDITOR
unset CDPATH
unset GREP_OPTIONS

setup () {
    rm -rf scrap;
    mkdir scrap;
}
