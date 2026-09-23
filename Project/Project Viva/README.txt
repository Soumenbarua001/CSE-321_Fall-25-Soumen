CSE321: Operating Systems
SimpleFS Lab Term Project - Summer 2026

Lab Section: 4
Group Number: 7

Group Members:
1. M Hafizur Rahman Shuchi - 24341203
2. Soumen Barua - 22201738
3. Tasmia Huda - 22341055

===============================================================
COMPILATION
===============================================================
gcc -Wall -Wextra -std=c11 simplefs_builder.c -o simplefs_builder
gcc -Wall -Wextra -std=c11 simplefs_adder.c -o simplefs_adder

No warnings on either file with -Wall -Wextra -std=c11.

===============================================================
EXECUTION EXAMPLES
===============================================================
Create an empty image:
    ./simplefs_builder --image disk.img
    ls -l disk.img          # should be 262144 bytes

Add a file (must be in the same directory you're running from):
    ./simplefs_adder --input disk.img --file test1.txt
    ./simplefs_adder --input disk.img --file test2.txt
    ./simplefs_adder --input disk.img --file test3.txt

Check the image:
    xxd disk.img
    hexdump -C disk.img

===============================================================
IMPLEMENTATION DESCRIPTION
===============================================================
simplefs_builder.c

Zero-fills all 64 blocks first (262144 bytes total), then writes the
superblock into Block 0 - magic number, block size, total blocks,
inode count, and the block numbers for the bitmaps/inode table/data
region. After that it sets bit 0 in both the inode bitmap and the
data bitmap (root inode + root's data block), writes the root inode
itself into Block 3 (directory, links = 2, size = 128, direct[0] = 4),
and finally puts the "." and ".." entries at the start of Block 4.
Pretty much a direct translation of the layout given in the spec.

simplefs_adder.c

This one does a lot more. First it checks the image's magic number and
grabs the source file's size, then rejects it if it's over 12288 bytes
or the filename is longer than 58 chars. Number of blocks needed is
just ceil(file_size / 4096). Before allocating anything it checks the
root directory doesn't already have a file with that name.

Allocation is first-fit - scans the inode bitmap starting at index 1
and the data bitmap from 0, and every block we grab gets marked in the
in-memory bitmap right away so we don't hand out the same block twice
in the same run. File contents get copied block by block into a
zeroed 4096-byte buffer each time, so the leftover space in the last
block is guaranteed to be zero and not leftover garbage from the
buffer.

Once the data's on disk, it builds the inode (type = file, links = 1,
real size, block numbers in direct[0..2]) and writes it to the inode
table, updates both bitmaps on disk, then finds the first open slot in
the root directory (starting from index 2, since 0 and 1 are "." and
"..") and writes the new entry there. Last step is bumping the root
inode's size by 64 bytes. Source file is only ever opened "rb" so it's
never touched.

===============================================================
CONTRIBUTION OF EACH GROUP MEMBER
===============================================================
M Hafizur Rahman Shuchi (24341203):
    - Wrote simplefs_builder.c and simplefs_adder.c end to end.
    - Ran through all the test cases from the spec (empty image,
      superblock/bitmap checks, 1/2/3-block files, duplicate file,
      oversized file, missing file/image, etc.) and checked disk.img
      with xxd after each one.
    - Put together this README.

Soumen Barua (22201738):
    - Went through the struct definitions in simplefs.h line by line
      against the spec to make sure sizes/offsets/padding for the
      superblock, inode, and dirent all lined up (Sections 6-13).
    - Double checked the inode-number <-> table-index and the
      data-bitmap-index <-> absolute-block-number math used in
      simplefs_adder.c since that part is easy to get off-by-one on.
    - Reran the duplicate-file, oversized-file, two-block, and
      missing-source/image tests separately to confirm the same
      results.

Tasmia Huda (22341055):
    - Checked the root directory setup (Block 4, the "." / ".."
      entries) and the "no free space left" error paths in
      simplefs_adder.c against what the spec expects.
    - Reran the empty-image and single-block-file tests and compared
      the hex dump against the expected layout.
    - Proofread this README and caught a couple of section numbers
      that didn't match the spec.

===============================================================
KNOWN LIMITATIONS / PROBLEMS
===============================================================
- Only the root directory is supported (no subdirectories, indirect
  blocks, deletion, renaming, links, permissions, journaling,
  checksums, mounting, or caching) - this is by design per the spec,
  not something we ran out of time for.
- Max file size is 12288 bytes (3 direct blocks) and max regular files
  is 31 (32 inodes - 1 for root).
- No bugs we're aware of. All the test cases in Sections 19-20 of the
  spec passed against the compiled binaries.
