#include "exfs2.h"

/**
 * Maps a global block number to a segment index and block index within the segment.
 */
void get_segment_and_block_offset(int global_block_num, int *segment_idx, int *block_offset) {
    // Special case: block 0 is reserved
    if (global_block_num == 0) {
        *segment_idx = 0;
        *block_offset = 0;
        return;
    }

    *segment_idx = global_block_num / BLOCKS_PER_SEGMENT;
    *block_offset = global_block_num % BLOCKS_PER_SEGMENT;

    if (*segment_idx >= num_data_segments || data_segments[*segment_idx] == NULL) {
        fprintf(stderr, "[offset-error] Invalid segment index %d for block %d (max %d)\n",
                *segment_idx, global_block_num, num_data_segments - 1);
        exit(EXIT_FAILURE);
    }
}

/**
 * Reads an indirect block and extracts a list of block numbers.
 */
void extract_block_list(uint32_t block_num, uint32_t *out_blocks, size_t max_blocks) {
    int seg, blk;
    get_segment_and_block_offset(block_num, &seg, &blk);

    if (seg < 0 || seg >= num_data_segments || data_segments[seg] == NULL) {
        fprintf(stderr, "[helpers] ERROR: Invalid segment index %d for block %u (max %d)\n",
                seg, block_num, num_data_segments - 1);
        memset(out_blocks, 0, max_blocks * sizeof(uint32_t));  // Avoid garbage
        return;
    }

    fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
    fread(out_blocks, sizeof(uint32_t), max_blocks, data_segments[seg]);
}

/**
 * Extracts blocks listed in an indirect block and writes content to stdout.
 */
void extract_indirect_block(uint32_t block_num, uint32_t *remaining) {
    int seg, blk;
    get_segment_and_block_offset(block_num, &seg, &blk);

    if (seg < 0 || seg >= num_data_segments || data_segments[seg] == NULL) {
        fprintf(stderr, "[helpers] ERROR: Invalid segment index %d for indirect block %u\n", seg, block_num);
        return;
    }

    uint32_t pointers[PTRS_PER_BLOCK] = {0};
    fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
    fread(pointers, sizeof(uint32_t), PTRS_PER_BLOCK, data_segments[seg]);

    for (size_t i = 0; i < PTRS_PER_BLOCK && *remaining > 0; ++i) {
        if (pointers[i] == 0) break;

        int data_seg, data_blk;
        get_segment_and_block_offset(pointers[i], &data_seg, &data_blk);

        uint8_t buffer[BLOCK_SIZE];
        uint32_t to_read = (*remaining > BLOCK_SIZE) ? BLOCK_SIZE : *remaining;

        fseek(data_segments[data_seg], data_blk * BLOCK_SIZE, SEEK_SET);
        fread(buffer, 1, to_read, data_segments[data_seg]);
        fwrite(buffer, 1, to_read, stdout);

        *remaining -= to_read;
    }
}

/**
 * Adds a new file entry into a directory's data block.
 */
void update_directory_entry(uint32_t parent_inode_num, uint32_t new_inode_num, const char *filename) {
    int seg, off;
    get_segment_and_inode_offset(parent_inode_num, &seg, &off);

    Inode parent;
    fseek(inode_segments[seg], off * sizeof(Inode), SEEK_SET);
    fread(&parent, sizeof(Inode), 1, inode_segments[seg]);

    get_segment_and_block_offset(parent.direct[0], &seg, &off);

    char block[BLOCK_SIZE];
    fseek(data_segments[seg], off * BLOCK_SIZE, SEEK_SET);
    fread(block, BLOCK_SIZE, 1, data_segments[seg]);

    int dir_offset = 0;
    while (dir_offset < BLOCK_SIZE) {
        DirEntry *entry = (DirEntry *)(block + dir_offset);
        if (entry->inode_num == 0 || entry->name_len == 0) break;
        dir_offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
    }

    DirEntry new_entry = {0};
    new_entry.inode_num = new_inode_num;
    new_entry.name_len = (uint8_t)strlen(filename);
    strncpy(new_entry.name, filename, MAX_NAME_LEN);

    memcpy(block + dir_offset, &new_entry.inode_num, sizeof(uint32_t));
    memcpy(block + dir_offset + sizeof(uint32_t), &new_entry.name_len, sizeof(uint8_t));
    memcpy(block + dir_offset + sizeof(uint32_t) + sizeof(uint8_t),
           new_entry.name, new_entry.name_len + 1);

    fseek(data_segments[seg], off * BLOCK_SIZE, SEEK_SET);
    fwrite(block, BLOCK_SIZE, 1, data_segments[seg]);
    fflush(data_segments[seg]);
}
