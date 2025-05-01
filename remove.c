#include "exfs2.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/**
 * Remove a file from the file system and free all its associated blocks.
 */
void run_remove(const char *exfs_path) {
    fprintf(stderr, "[remove] Removing '%s'\n", exfs_path);

    char path_copy[MAX_PATH];
    strncpy(path_copy, exfs_path, MAX_PATH - 1);
    path_copy[MAX_PATH - 1] = '\0';

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash || *(last_slash + 1) == '\0') {
        fprintf(stderr, "[remove] Invalid path: %s\n", exfs_path);
        return;
    }

    char *filename = last_slash + 1;
    *last_slash = '\0';  // Isolate parent path

    int parent_inode_num = find_inode_by_path(path_copy[0] ? path_copy : "/");
    if (parent_inode_num < 0) {
        fprintf(stderr, "[remove] Parent directory not found\n");
        return;
    }

    int parent_seg, parent_off;
    get_segment_and_inode_offset(parent_inode_num, &parent_seg, &parent_off);

    Inode parent;
    fseek(inode_segments[parent_seg], parent_off * sizeof(Inode), SEEK_SET);
    fread(&parent, sizeof(Inode), 1, inode_segments[parent_seg]);

    int blk_seg, blk_off;
    get_segment_and_block_offset(parent.direct[0], &blk_seg, &blk_off);

    char dir_block[BLOCK_SIZE];
    fseek(data_segments[blk_seg], blk_off * BLOCK_SIZE, SEEK_SET);
    fread(dir_block, BLOCK_SIZE, 1, data_segments[blk_seg]);

    uint32_t offset = 0, found_offset = UINT32_MAX, target_inode_num = UINT32_MAX;

    while (offset < BLOCK_SIZE) {
        DirEntry *entry = (DirEntry *)(dir_block + offset);
        if (entry->inode_num == 0 || entry->name_len == 0) break;

        if (entry->name_len == strlen(filename) &&
            strncmp(entry->name, filename, entry->name_len) == 0) {
            target_inode_num = entry->inode_num;
            found_offset = offset;
            break;
        }

        offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
    }

    if (target_inode_num == UINT32_MAX || found_offset == UINT32_MAX) {
        fprintf(stderr, "[remove] File '%s' not found in parent dir\n", filename);
        return;
    }

    // Remove directory entry
    memset(dir_block + found_offset, 0, sizeof(DirEntry));
    fseek(data_segments[blk_seg], blk_off * BLOCK_SIZE, SEEK_SET);
    fwrite(dir_block, BLOCK_SIZE, 1, data_segments[blk_seg]);
    fflush(data_segments[blk_seg]);

    // Load and clear the file inode
    int inode_seg, inode_off;
    get_segment_and_inode_offset(target_inode_num, &inode_seg, &inode_off);

    Inode file_inode;
    fseek(inode_segments[inode_seg], inode_off * sizeof(Inode), SEEK_SET);
    fread(&file_inode, sizeof(Inode), 1, inode_segments[inode_seg]);

    char zero_block[BLOCK_SIZE] = {0};

    // --- Direct blocks ---
    for (uint32_t i = 0; i < DIRECT_BLOCKS; ++i) {
        if (file_inode.direct[i] != 0) {
            int seg, blk;
            get_segment_and_block_offset(file_inode.direct[i], &seg, &blk);
            fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
            fwrite(zero_block, BLOCK_SIZE, 1, data_segments[seg]);
        }
    }

    // --- Single Indirect ---
    if (file_inode.indirect_single != 0) {
        uint32_t blocks[PTRS_PER_BLOCK] = {0};
        extract_block_list(file_inode.indirect_single, blocks, PTRS_PER_BLOCK);

        for (uint32_t i = 0; i < PTRS_PER_BLOCK; ++i) {
            if (blocks[i] == 0) break;
            int seg, blk;
            get_segment_and_block_offset(blocks[i], &seg, &blk);
            fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
            fwrite(zero_block, BLOCK_SIZE, 1, data_segments[seg]);
        }

        int seg, blk;
        get_segment_and_block_offset(file_inode.indirect_single, &seg, &blk);
        fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
        fwrite(zero_block, BLOCK_SIZE, 1, data_segments[seg]);
    }

    // --- Double Indirect ---
    if (file_inode.indirect_double != 0) {
        uint32_t dbl[PTRS_PER_BLOCK] = {0};
        extract_block_list(file_inode.indirect_double, dbl, PTRS_PER_BLOCK);

        for (uint32_t i = 0; i < PTRS_PER_BLOCK; ++i) {
            if (dbl[i] == 0) break;

            uint32_t inner[PTRS_PER_BLOCK] = {0};
            extract_block_list(dbl[i], inner, PTRS_PER_BLOCK);

            for (uint32_t j = 0; j < PTRS_PER_BLOCK; ++j) {
                if (inner[j] == 0) break;
                int seg, blk;
                get_segment_and_block_offset(inner[j], &seg, &blk);
                fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
                fwrite(zero_block, BLOCK_SIZE, 1, data_segments[seg]);
            }

            int seg, blk;
            get_segment_and_block_offset(dbl[i], &seg, &blk);
            fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
            fwrite(zero_block, BLOCK_SIZE, 1, data_segments[seg]);
        }

        int seg, blk;
        get_segment_and_block_offset(file_inode.indirect_double, &seg, &blk);
        fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
        fwrite(zero_block, BLOCK_SIZE, 1, data_segments[seg]);
    }

    // Clear the inode itself
    Inode empty = {0};
    fseek(inode_segments[inode_seg], inode_off * sizeof(Inode), SEEK_SET);
    fwrite(&empty, sizeof(Inode), 1, inode_segments[inode_seg]);
    fflush(inode_segments[inode_seg]);

    fprintf(stderr, "[remove] File '%s' removed successfully.\n", filename);
}
