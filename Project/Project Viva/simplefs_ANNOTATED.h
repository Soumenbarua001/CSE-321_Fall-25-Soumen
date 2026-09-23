#ifndef SIMPLEFS_H                 // include guard start - stops this header
#define SIMPLEFS_H                 // from being pasted in twice by the compiler

#include <stdio.h>                 // FILE*, fopen/fread/fwrite/fseek/printf
#include <stdint.h>                // fixed-width types uint32_t/uint16_t/uint8_t
                                    // (guarantees exact byte sizes -> needed for
                                    // a binary on-disk format)
#include <stdlib.h>                // general utilities (brought in by convention)
#include <string.h>                // strcmp, strcpy, strncpy, memset, strlen

#define BLOCK_SIZE 4096            // every block on the "disk" is 4096 bytes
#define TOTAL_BLOCKS 64            // 64 x 4096 = 262144 bytes = required image size
#define TOTAL_INODES 32            // 32 inode slots exist; inode 1 is root, so 31
                                    // are actually available for files
#define SUPERBLOCK_BLOCK 0         // Block 0  -> superblock lives here
#define INODE_BITMAP_BLOCK 1       // Block 1  -> 1 bit per inode (32 bits used)
#define DATA_BITMAP_BLOCK 2        // Block 2  -> 1 bit per data block (60 used)
#define INODE_TABLE_BLOCK 3        // Block 3  -> 32 inodes x 128 bytes = 4096 bytes,
                                    // fits exactly in one block
#define DATA_REGION_BLOCK 4        // Blocks 4..63 -> actual file/dir data
#define DATA_BLOCKS 60             // 64 total - 4 reserved (super+2 bitmaps+table)
                                    // = 60 blocks left for data
#define ROOT_INODE 1               // inode numbers start at 1, not 0; root is #1
#define ROOT_DATA_BLOCK 4          // root's one and only data block is Block 4
#define MAGIC_NUMBER 0x53465331    // fingerprint written to superblock so the
                                    // adder can verify "is this really a SimpleFS
                                    // image?"; spells "SFS1" in ASCII hex
#define TYPE_FILE 1                // tag used in inode_t.type / dirent_t.type
#define TYPE_DIRECTORY 2           // tag used in inode_t.type / dirent_t.type
#define MAX_DIRECT_BLOCKS 3        // every inode has exactly 3 direct pointers
                                    // and nothing else (no indirect blocks)
#define MAX_FILE_SIZE (MAX_DIRECT_BLOCKS * BLOCK_SIZE)   // 3*4096 = 12288 bytes,
                                    // largest file SimpleFS can store
#define DIRENT_SIZE 64             // size in bytes of one directory-entry record
                                    // (matches sizeof(dirent_t) below)

typedef struct {
    uint32_t magic, block_size, total_blocks, inode_count;
    // magic         -> checked against MAGIC_NUMBER by the adder
    // block_size    -> repeats BLOCK_SIZE onto disk
    // total_blocks  -> repeats TOTAL_BLOCKS onto disk
    // inode_count   -> repeats TOTAL_INODES onto disk
    uint32_t inode_bitmap_block, data_bitmap_block, inode_table_block, data_region_block;
    // repeats the four fixed block numbers onto disk, so a reader could in
    // theory re-derive the layout instead of hardcoding it a second time
    uint32_t root_inode;
    // which inode number is root (always 1 in this project)
} superblock_t;                    // 9 x uint32_t = 36 bytes total, written once
                                    // into Block 0 by the builder

typedef struct {
    uint16_t type, links;
    // type  -> TYPE_FILE or TYPE_DIRECTORY
    // links -> how many directory entries point at this inode (root starts
    //          at 2 because of "." and ".."; a regular file gets 1)
    uint32_t size;
    // exact byte size of the file, or for a directory, total bytes used by
    // its entries
    uint32_t direct[3];
    // absolute block numbers holding this file/dir's data - the ONLY way to
    // find a file's actual bytes on disk
    uint8_t reserved[108];
    // unused padding. 2+2+4+(4*3)=20 "real" bytes, 20+108=128 -> forces the
    // whole struct to be exactly 128 bytes (see VIVA POINT below)
} inode_t;
// VIVA POINT: why 128 bytes exactly? 4096 (one block) / 128 = 32, which
// exactly equals TOTAL_INODES - that's why the whole inode table fits
// perfectly into Block 3 with zero wasted space.

typedef struct {
    uint32_t inode_no;
    // which inode this name points to; 0 means "this slot is empty/free" -
    // this convention is used everywhere a free directory slot is searched
    uint8_t type;
    // TYPE_FILE/TYPE_DIRECTORY, lets you tell the type without opening the
    // inode it points to
    char name[59];
    // null-terminated filename: 58 usable characters + 1 byte for the
    // required null terminator
} dirent_t;
// Total size: 4+1+59 = 64 bytes = DIRENT_SIZE.
// VIVA POINT: why 64 bytes? 4096 / 64 = 64 entries per block - that's why
// one 4096-byte data block can hold up to 64 directory entries for root.

// Shared prototypes. Both simplefs_builder.c and simplefs_adder.c provide
// their OWN definitions of these - the header only promises they exist.
void set_bit(unsigned char *bitmap, int index);
int is_bit_set(unsigned char *bitmap, int index);
int find_free_inode(unsigned char *bitmap);
// builder's version is a dummy stub (always -1, never called) because the
// builder always creates a brand-new empty image and never needs to search
// for free space; adder's version does the real first-fit search.
int find_free_data_block(unsigned char *bitmap);
// same story as find_free_inode - stub in builder, real search in adder
long inode_offset(int inode_number);
// computes the byte offset of a given inode's record in the inode table
#endif                              // closes the include guard from line 1
