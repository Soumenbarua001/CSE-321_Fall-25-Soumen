#include "simplefs.h"

void set_bit(unsigned char *bitmap, int index) { bitmap[index / 8] |= (1u << (index % 8)); }
// identical logic to simplefs_builder.c - MUST match since both programs
// read/write the same on-disk bitmap layout
int is_bit_set(unsigned char *bitmap, int index) { return bitmap[index / 8] & (1u << (index % 8)); }
long inode_offset(int inode_number) { return ((long)INODE_TABLE_BLOCK * BLOCK_SIZE) + ((long)(inode_number - 1) * sizeof(inode_t)); }
int data_bitmap_index(int absolute_block_number) { return absolute_block_number - DATA_REGION_BLOCK; }
// converts an ABSOLUTE block number (like the ones stored in direct[0..2],
// e.g. 4..63) into a bitmap-RELATIVE index (0..59) by subtracting
// DATA_REGION_BLOCK. Needed because the data bitmap only has 60 usable
// bits, counting from 0, while the rest of the program talks in absolute
// block numbers.

int find_free_inode(unsigned char *bitmap)
{
    /* TODO 1: Search bitmap indexes 1..31 and return INODE NUMBER. */
    /* TODO: STUDENT CODE START */
    for (int i = 1; i < TOTAL_INODES; i++) {
        // starts at i=1, NOT 0, because bitmap index 0 (inode #1 = root)
        // is always permanently allocated and must never be reused
        if (!is_bit_set(bitmap, i)) return i + 1;
        // first-fit: return the FIRST free bit found, +1 to convert the
        // 0-based bitmap index back into a 1-based inode number
    }
    /* TODO: STUDENT CODE END */
    return -1;   // reached only if all 31 usable inodes are taken
}

int find_free_data_block(unsigned char *bitmap)
{
    /* TODO 2: First-fit search; return ABSOLUTE data block number. */
    /* TODO: STUDENT CODE START */
    for (int i = 0; i < DATA_BLOCKS; i++) {
        // loops over all 60 usable data-block bits
        if (!is_bit_set(bitmap, i)) return DATA_REGION_BLOCK + i;
        // first-fit again: DATA_REGION_BLOCK + i converts the relative
        // index back into the absolute block number (e.g. index 0 -> 4)
    }
    /* TODO: STUDENT CODE END */
    return -1;   // reached only if all 60 data blocks are full
}

int filename_exists(FILE *image, const char *filename)
{
    dirent_t entry;
    /* TODO 3: Search root directory entries for filename. */
    /* TODO: STUDENT CODE START */
    fseek(image, (long)ROOT_DATA_BLOCK * BLOCK_SIZE, SEEK_SET);
    // reads DIRECTLY from the image file on disk, not an in-memory copy
    for (int i = 0; i < BLOCK_SIZE / (int)sizeof(dirent_t); i++) {
        // 4096 / 64 = 64 slots total that fit in root's one data block
        if (fread(&entry, sizeof(entry), 1, image) != 1) break;
        // read one entry at a time, sequentially
        if (entry.inode_no != 0 && strcmp(entry.name, filename) == 0) return 1;
        // inode_no != 0 means the slot is in use; if the name also
        // matches, it's a duplicate -> return 1 immediately
    }
    /* TODO: STUDENT CODE END */
    return 0;   // no match found after scanning all slots
}

int find_free_directory_entry(FILE *image)
{
    dirent_t entry;
    /* TODO 4: Search entries 2..63; free entry has inode_no == 0. */
    /* TODO: STUDENT CODE START */
    for (int i = 2; i < BLOCK_SIZE / (int)sizeof(dirent_t); i++) {
        // starts at index 2, skipping slots 0 and 1 which are always
        // "." and ".." and never free
        long pos = ((long)ROOT_DATA_BLOCK * BLOCK_SIZE) + ((long)i * sizeof(dirent_t));
        // computes this slot's exact byte offset directly (rather than
        // reading sequentially), since we need to know WHICH index is free
        fseek(image, pos, SEEK_SET);
        if (fread(&entry, sizeof(entry), 1, image) != 1) break;
        if (entry.inode_no == 0) return i;
        // inode_no == 0 is the "this slot is empty" convention used
        // throughout the project
    }
    /* TODO: STUDENT CODE END */
    return -1;   // all 64 slots in the block are full -> directory is full
}

int main(int argc, char *argv[])
{
    char *image_name = NULL, *source_name = NULL;   // the two filenames from argv
    FILE *image, *source;                            // file handles
    superblock_t sb;                                  // superblock read back from disk
    unsigned char inode_bitmap[BLOCK_SIZE], data_bitmap[BLOCK_SIZE];
    // in-memory working copies, loaded from disk, mutated as we allocate,
    // then written back to disk once at the end
    inode_t new_inode, root_inode;    // new file's inode, and root's inode (re-read/updated)
    dirent_t new_entry;                // the new directory entry being created
    long file_size;                    // size of the source file in bytes
    int required_blocks, free_inode;   // how many blocks needed; which inode chosen
    int allocated_blocks[MAX_DIRECT_BLOCKS] = {0};
    // remembers which blocks got allocated this run, in order (up to 3)
    int directory_entry_index;         // which root-directory slot to use

    if (argc != 5) { printf("Usage: %s --input <image> --file <file>\n", argv[0]); return 1; }
    // must be exactly: program, "--input", <image>, "--file", <file> = 5 argv entries
    if (strcmp(argv[1], "--input") != 0 || strcmp(argv[3], "--file") != 0) { printf("Error: invalid command-line arguments.\n"); return 1; }
    // enforces both flags are literal and in the right position
    image_name = argv[2]; source_name = argv[4];

    image = fopen(image_name, "rb+");
    // "rb+" (unlike builder's "wb+") requires the file to ALREADY EXIST -
    // we're adding to an existing image, not creating a new one
    if (!image) { printf("Error: file-system image not found.\n"); return 1; }
    fseek(image, SUPERBLOCK_BLOCK * BLOCK_SIZE, SEEK_SET);
    if (fread(&sb, sizeof(sb), 1, image) != 1) { printf("Error: could not read superblock.\n"); fclose(image); return 1; }
    // reads the superblock back off disk; errors out if the read is short
    if (sb.magic != MAGIC_NUMBER) { printf("Error: invalid SimpleFS image.\n"); fclose(image); return 1; }
    // confirms this is really a valid SimpleFS image before touching it

    source = fopen(source_name, "rb");
    // read-only ("rb") - this source file is never modified
    if (!source) { printf("Error: source file not found.\n"); fclose(image); return 1; }
    // if missing, also closes the already-open image handle first (avoids
    // leaking a file descriptor)
    fseek(source, 0, SEEK_END); file_size = ftell(source); rewind(source);
    // standard C trick to get file size: jump to the end, ask "what byte
    // offset am I at" (that's the size), then rewind back to the start so
    // later freads actually read real data from the beginning
    if (file_size < 0) { printf("Error: could not determine source file size.\n"); fclose(source); fclose(image); return 1; }
    // defensive check in case ftell() failed
    if (file_size > MAX_FILE_SIZE) { printf("Error: file is too large for SimpleFS.\n"); fclose(source); fclose(image); return 1; }
    // rejects anything bigger than 12288 bytes (3 blocks) up front

    /* TODO 5: Calculate required_blocks. Zero-byte file uses zero blocks. */
    /* TODO: STUDENT CODE START */
    if (strlen(source_name) > 58) { printf("Error: file name exceeds 58 characters.\n"); fclose(source); fclose(image); return 1; }
    // rejects names that wouldn't fit in the 59-byte name field (58 usable
    // chars + 1 null terminator)
    required_blocks = (int)((file_size + BLOCK_SIZE - 1) / BLOCK_SIZE);
    // ceiling-division trick: normal integer division truncates (rounds
    // down), so adding BLOCK_SIZE-1 (4095) before dividing forces it to
    // round UP instead. 1 byte -> 1 block; exactly 4096 bytes -> still 1
    // block; 4097 bytes -> 2 blocks. A 0-byte file gives required_blocks=0.
    /* TODO: STUDENT CODE END */

    if (filename_exists(image, source_name)) { printf("Error: file already exists in SimpleFS.\n"); fclose(source); fclose(image); return 1; }
    // duplicate check done BEFORE any allocation, so a duplicate never
    // wastes allocation work

    fseek(image, INODE_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    fread(inode_bitmap, BLOCK_SIZE, 1, image);
    // loads the on-disk inode bitmap into memory
    free_inode = find_free_inode(inode_bitmap);
    if (free_inode == -1) { printf("Error: no free inode available.\n"); fclose(source); fclose(image); return 1; }
    // bails out if all 31 usable inodes are taken

    fseek(image, DATA_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    fread(data_bitmap, BLOCK_SIZE, 1, image);
    // loads the data bitmap into memory - THIS copy gets mutated below as
    // blocks are allocated, then written back to disk only once, later

    /* TODO 6: Allocate required data blocks and mark them in memory. */
    /* TODO: STUDENT CODE START */
    for (int i = 0; i < required_blocks; i++) {
        int block = find_free_data_block(data_bitmap);
        if (block == -1) { printf("Error: insufficient free data blocks.\n"); fclose(source); fclose(image); return 1; }
        set_bit(data_bitmap, data_bitmap_index(block));
        // CRITICAL: marks the block used IN MEMORY immediately, so the
        // very next loop iteration's find_free_data_block call won't
        // return this same block again
        allocated_blocks[i] = block;   // remember it for later steps
    }
    /* TODO: STUDENT CODE END */

    directory_entry_index = find_free_directory_entry(image);
    if (directory_entry_index == -1) { printf("Error: root directory is full.\n"); fclose(source); fclose(image); return 1; }
    // found up front (before writing any file data) so a full directory
    // fails fast without needing to undo any data already written

    /* TODO 7: Copy source contents into allocated blocks using zero-filled buffers. */
    /* TODO: STUDENT CODE START */
    {
        unsigned char buffer[BLOCK_SIZE];
        long remaining = file_size;   // bytes still left to copy
        for (int i = 0; i < required_blocks; i++) {
            memset(buffer, 0, BLOCK_SIZE);
            // buffer zeroed FIRST, every iteration - this is exactly what
            // guarantees the unused "tail" of the LAST block is written as
            // clean zeros, not garbage left over from a previous iteration
            size_t to_read = (remaining > BLOCK_SIZE) ? (size_t)BLOCK_SIZE : (size_t)remaining;
            // full 4096 for every block except possibly the last one
            if (to_read > 0 && fread(buffer, 1, to_read, source) != to_read) {
                // skips fread entirely when to_read==0 (e.g. loop wouldn't
                // even run for a 0-byte file since required_blocks=0)
                printf("Error: could not read source file.\n");
                fclose(source); fclose(image); return 1;
            }
            fseek(image, (long)allocated_blocks[i] * BLOCK_SIZE, SEEK_SET);
            fwrite(buffer, BLOCK_SIZE, 1, image);
            // writes the WHOLE 4096-byte buffer (real data + zero padding)
            remaining -= (long)to_read;
        }
    }
    /* TODO: STUDENT CODE END */

    /* TODO 8: Initialize new file inode and its direct pointers. */
    memset(&new_inode, 0, sizeof(new_inode));   // zero first, same reasoning as builder
    /* TODO: STUDENT CODE START */
    new_inode.type = TYPE_FILE;
    new_inode.links = 1;          // only the one new directory entry points to it
    new_inode.size = (uint32_t)file_size;   // the REAL byte count, not block-rounded
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        new_inode.direct[i] = (i < required_blocks) ? (uint32_t)allocated_blocks[i] : 0;
        // fills real block numbers only where needed, leaves the rest
        // cleanly at 0 (unused) - this ternary is the key line here
    }
    /* TODO: STUDENT CODE END */
    fseek(image, inode_offset(free_inode), SEEK_SET);
    fwrite(&new_inode, sizeof(new_inode), 1, image);
    // writes the new inode into its slot in the inode table

    /* TODO 9: Mark allocated inode in inode bitmap. */
    /* TODO: STUDENT CODE START */
    set_bit(inode_bitmap, free_inode - 1);
    // "-1" converts the 1-based inode NUMBER into a 0-based bitmap INDEX,
    // same convention as inode_offset()
    /* TODO: STUDENT CODE END */
    fseek(image, INODE_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    fwrite(inode_bitmap, BLOCK_SIZE, 1, image);
    fseek(image, DATA_BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET);
    fwrite(data_bitmap, BLOCK_SIZE, 1, image);
    // this is the ONE point where both bitmaps actually get committed to
    // disk - the data bitmap changes from the allocation loop were only
    // in memory until now

    /* TODO 10: Create directory entry; ensure name is null-terminated. */
    memset(&new_entry, 0, sizeof(new_entry));
    /* TODO: STUDENT CODE START */
    new_entry.inode_no = (uint32_t)free_inode;
    new_entry.type = TYPE_FILE;
    strncpy(new_entry.name, source_name, sizeof(new_entry.name) - 1);
    // bounded copy - can never overflow the 59-byte buffer; because memset
    // already zeroed the whole struct, a null terminator is guaranteed
    // even in the edge case of a 58-character name
    /* TODO: STUDENT CODE END */
    {
        long pos = ((long)ROOT_DATA_BLOCK * BLOCK_SIZE) + ((long)directory_entry_index * sizeof(dirent_t));
        fseek(image, pos, SEEK_SET);
        fwrite(&new_entry, sizeof(new_entry), 1, image);
        // writes the new entry into its chosen free slot
    }

    fseek(image, inode_offset(ROOT_INODE), SEEK_SET);
    fread(&root_inode, sizeof(root_inode), 1, image);
    // re-reads root's CURRENT inode from disk before modifying it (in case
    // it changed from a previous run of the adder)

    /* TODO 11: Increase root_inode.size by sizeof(dirent_t). */
    /* TODO: STUDENT CODE START */
    root_inode.size += (uint32_t)sizeof(dirent_t);
    // bumps root's recorded size by exactly 64 bytes, since one more
    // directory entry now exists
    /* TODO: STUDENT CODE END */
    fseek(image, inode_offset(ROOT_INODE), SEEK_SET);
    fwrite(&root_inode, sizeof(root_inode), 1, image);
    // writes the updated root inode back to its fixed slot

    fclose(source); fclose(image);
    printf("%s added successfully to %s\n", source_name, image_name);
    return 0;
}
