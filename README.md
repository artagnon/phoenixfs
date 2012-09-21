# phoenixfs v0.1
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

* /tmp/phoenixfs.log is the debug log

master.idx, master.pack, and fstree are enough to recreate the entire
versioned filesystem.  The files in gitdir/ and gitdir/.git/loose/ can
be removed after unmount.

Notes on the filesystem tree:

(dr: directory record | fr: file record | vfr: versioned file record)

* dr just contains a name and node pointer referencing vfrs.  drs are
  inserted directly into the root node.

* vfr contains the path of the file, a list of frs representing the
  various versions of the file (fixed at REV_TRUNCATE), and a HEAD
  pointer to keep track of the latest version of the file.

* The B+ tree is keyed by the CRC32 hash of the path of the vfr/ dr, a
  design decision inspired by Btrfs.

* An fr, vfr, and dr (corresponding to its path) are created when a
  new file is created on the filesystem.  Empty directories are not
  tracked: no dr is created for empty directories.

## Limitations
The following invocations don't work:

     $ cp file1@1 file1 # FILE@REV can't be treated like a file

## Contributing
Simply fork the project on GitHub and send pull requests.
