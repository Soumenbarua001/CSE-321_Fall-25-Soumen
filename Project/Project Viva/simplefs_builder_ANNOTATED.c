#include "simplefs.h"               // pulls in all constants/structs/prototypes

void set_bit(unsigned char *bitmap, int index) {
    bitmap[index / 8] |= (1u << (index % 8));
    // index/8      -> which BYTE holds this bit (8 bits per byte)
    // index % 8    -> which bit position (0-7) inside that byte
    // 1u << (...)  -> builds a mask with only that one bit set
    // |=           -> turns that bit ON without touching any other bit
}
int is_bit_set(unsigned char *bitmap, int index) {
    return bitmap[index / 8] & (1u << (index % 8));
    // same addressing as set_bit, but uses & to TEST the bit instead of
    // setting it; returns non-zero if that bit is currently 1 (allocated)
}
long inode_offset(int inode_number) {
    return ((long)INODE_TABLE_BLOCK * BLOCK_SIZE) + ((long)(inode_number - 1) * sizeof(inode_t));
    // (INODE_TABLE_BLOCK * BLOCK_SIZE) -> byte offset of the start of Block 3
    // (inode_number - 1) * sizeof(inode_t) -> the "-1" converts a 1-based
    // inode NUMBER into a 0-based array INDEX, since the table is stored
    // like a plain array but inode numbering starts at 1
}
int find_free_inode(unsigned char *bitmap) { (void)bitmap; return -1; }
int find_free_data_block(unsigned char *bitmap) { (void)bitmap; return -1; }
// Builder never needs to search for free space (it always builds a brand
// new, empty image) - these stub definitions exist only to satisfy the
// prototypes declared in simplefs.h. (void)bitmap; silences the "unused
// parameter" warning under -Wextra.

int main(int argc, char *argv[])
{
    char *image_name = NULL;               // will hold argv[2], the output filename
    FILE *fp;                              // the image file handle
    unsigned char zero_block[BLOCK_SIZE] = {0};   // all-zero buffer, used to blank
                                                   // out every block of the image
    unsigned char inode_bitmap[BLOCK_SIZE] = {0}; // starts fully zeroed (all free)
    unsigned char data_bitmap[BLOCK_SIZE] = {0};  // starts fully zeroed (all free)
    superblock_t sb;                        // superblock struct to be filled in
    inode_t root_inode;                     // root directory's inode
    dirent_t dot, dotdot;                   // the "." and ".." directory entries

    if (argc != 3) { printf("Usage: %s --image <image_name>\n", argv[0]); return 1; }
    // must be exactly: program name, "--image", filename -> 3 argv entries total
    if (strcmp(argv[1], "--image") != 0) { printf("Error: expected --image option.\n"); return 1; }
    // enforces the flag is literally "--image", not just any second argument
    image_name = argv[2];                  // save the filename the user wants created

    fp = fopen(image_name, "wb+");
    // "wb+" creates the file if missing (or truncates it if it exists) and
    // opens it for both writing and later seeking/reading
    if (!fp) { printf("Error: could not create image file.\n"); return 1; }

    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (fwrite(zero_block, BLOCK_SIZE, 1, fp) != 1) { printf("Error: could not initialize image.\n"); fclose(fp); return 1; }
    }
    // writes 64 x 4096 = 262144 zero bytes = the ENTIRE image, before anything
    // meaningful is written. Guarantees every bitmap bit, every unused inode
    // slot, and every unused data block starts as a clean, deterministic 0 -
    // which is exactly what the adder's "find free X" logic depends on.

    /* TODO 1: Fill all superblock fields. */
    memset(&sb, 0, sizeof(sb));
    // zeroes the whole struct first, so no field (or padding byte, if any)
    // is left as random leftover stack memory before we fill it in
    /* TODO: STUDENT CODE START */
    sb.magic = MAGIC_NUMBER;               // the "SFS1" fingerprint
    sb.block_size = BLOCK_SIZE;            // repeats 4096 onto disk
    sb.total_blocks = TOTAL_BLOCKS;        // repeats 64 onto disk
    sb.inode_count = TOTAL_INODES;         // repeats 32 onto disk
    sb.inode_bitmap_block = INODE_BITMAP_BLOCK;   // = 1
    sb.data_bitmap_block = DATA_BITMAP_BLOCK;     // = 2
    sb.inode_table_block = INODE_TABLE_BLOCK;     // = 3
    sb.data_region_block = DATA_REGION_BLOCK;     // = 4
    sb.root_inode = ROOT_INODE;            // = 1
    /* TODO: STUDENT CODE END */
    fseek(fp, SUPERBLOCK_BLOCK * BLOCK_SIZE, SEEK_SET);   // jump to byte 0 (Block 0)
    fwrite(&sb, sizeof(sb), 1, fp);                       // write the 36-byte struct

    /* TODO 2: Mark inode 1 allocated (inode bitmap index 0). */
    /* TODO: STUDENT CODE START */
    set_bit(inode_bitmap, 0);
    // marks bit 0 (= inode #1, root) allocated IN MEMORY, so the adder will
    // never think inode 1 is free and hand it out to a new file
    /* TODO: STUDENT CODE END */
    fseek(fp, INODE_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);  // jump to Block 1
    fwrite(inode_bitmap, BLOCK_SIZE, 1, fp);                // write it to disk

    /* TODO 3: Mark root data block allocated (data bitmap index 0). */
    /* TODO: STUDENT CODE START */
    set_bit(data_bitmap, 0);
    // marks bit 0 (= absolute Block 4, root's own data block) allocated
    /* TODO: STUDENT CODE END */
    fseek(fp, DATA_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);   // jump to Block 2
    fwrite(data_bitmap, BLOCK_SIZE, 1, fp);                 // write it to disk

    /* TODO 4: Initialize root inode according to the specification. */
    memset(&root_inode, 0, sizeof(root_inode));   // zero it first, same reasoning as sb
    /* TODO: STUDENT CODE START */
    root_inode.type = TYPE_DIRECTORY;      // root is a directory
    root_inode.links = 2;                  // one for ".", one fixed "extra" link a
                                            // directory has by spec convention
    root_inode.size = 2 * DIRENT_SIZE;     // 128 bytes: "." and ".." already exist
    root_inode.direct[0] = ROOT_DATA_BLOCK;// root's data lives in Block 4
    root_inode.direct[1] = 0;              // unused
    root_inode.direct[2] = 0;              // unused
    /* TODO: STUDENT CODE END */
    fseek(fp, inode_offset(ROOT_INODE), SEEK_SET);   // jump to root's slot in the table
    fwrite(&root_inode, sizeof(root_inode), 1, fp);  // write it (Block 3)

    /* TODO 5: Initialize the '.' entry. */
    memset(&dot, 0, sizeof(dot));
    /* TODO: STUDENT CODE START */
    dot.inode_no = ROOT_INODE;             // "." always points to the dir itself
    dot.type = TYPE_DIRECTORY;
    strcpy(dot.name, ".");                 // safe: "." is far shorter than 59 bytes
    /* TODO: STUDENT CODE END */

    /* TODO 6: Initialize the '..' entry. */
    memset(&dotdot, 0, sizeof(dotdot));
    /* TODO: STUDENT CODE START */
    dotdot.inode_no = ROOT_INODE;
    // ".." also points to root, because SimpleFS only ever has a root
    // directory - there's no parent to point to
    dotdot.type = TYPE_DIRECTORY;
    strcpy(dotdot.name, "..");
    /* TODO: STUDENT CODE END */

    fseek(fp, ROOT_DATA_BLOCK * BLOCK_SIZE, SEEK_SET);   // jump to Block 4
    fwrite(&dot, sizeof(dot), 1, fp);       // write "." into the first 64 bytes
    fwrite(&dotdot, sizeof(dotdot), 1, fp); // write ".." right after it (next 64 bytes)
    // everything after these 128 bytes in Block 4 stays zero (from the
    // earlier full-image zero-fill) - that's exactly why the adder's
    // "find a free directory slot" logic can treat inode_no==0 as "free"

    fclose(fp);                             // close the file
    printf("SimpleFS image created successfully: %s\n", image_name);
    return 0;                               // success exit code
}
