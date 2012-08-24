## phoenixfs v 0.1
A filesystem implemented in userspace (using FUSE), inspired by the
way Git is designed.  This is a very early implementation, and
contains lots of bugs.

## Dependencies
1. Zlib (>= 1.2)
2. FUSE (>= 2.6)
3. pkg-config (>= 0.25)
4. Linux kernel (>= 2.6.15)

## Usage
For the first run, you need two directories:
1. A git directory where the data will be stored.
2. An empty directory to use as the mountpoint.

    $ cd /tmp
    $ mkdir gitdir mountp
    $ phoneixfs mount gitdir mountp

Use the mountpoint as you see fit.  Data will be written to the gitdir
on umount: you can use it for subsequent mounts.

Now, everything in mountpoint is versioned.  To access older revisions
of FILE, use FILE@REV syntax, where REV is the number of revisions in
the past you want to access.  For example:

     $ echo "hello" >file1
     $ echo "goodbye" >file1
     $ echo "another hello" >file1
     $ cat file1
     another hello
     $ cat file1@1
     goodbye
     $ cat file1@2
     hello

Finally, to umount:

     $ fusermount -u mountp

## Technical documentation
Uses a B+ tree to keep track of the filesystem tree, and a modified
version of packfile v3/ packfile index v2 for storing revision
information.

* gitdir/.git/loose/ contains zlib-deflated versions of content blobs,
  named by the SHA-1 digest of the content.

* gitdir/fstree is a raw dump of the B+ tree in a custom format.

* gitdir/master.pack and gitdir/master.idx are the packfile and
  packfile index respectively.  During an unmount, the files in
  gitdir/.git/loose/ are packed up, and an index is generated.

A lot of the SHA-1 generation and packing code is copied from git.git.

## Limitations
The following invocations don't work:

     $ cp file1@1 file1 # FILE@REV can't be treated like a file
     $ echo "hello" >file1; echo "goodbye" >file1 # Race!

## Contributing
Simply fork the project on GitHub and send pull requests.
