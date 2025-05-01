#include "exfs2.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/**
 * Extract a file from the filesystem and write its content to stdout.
 */
void run_extract(const char *exfs_path) {
    fprintf(stderr, "[extract] Extracting '%s'\n", exfs_path);

    // Extract parent path and filename
    char parent_path[MAX_PATH];
    const char *filename = extract_path_tail(exfs_path, parent_path);
    if (!filename || strlen(filename) == 0) {
        fprintf(stderr, "[extract] Invalid path (missing filename)\n");
        return;
    }

    // Find parent directory inode
    int parent_inode = find_inode_by_path(parent_path[0] ? parent_path : "/");
    if (parent_inode < 0) {
        fprintf(stderr, "[extract] Parent directory '%s' not found\n", parent_path);
        return;
    }

    int parent_seg, parent_off;
    get_segment_and_inode_offset(parent_inode, &parent_seg, &parent_off);

    Inode parent;
    fseek(inode_segments[parent_seg], parent_off * sizeof(Inode), SEEK_SET);
    fread(&parent, sizeof(Inode), 1, inode_segments[parent_seg]);

    // Read the directory block
    int blk_seg, blk_off;
    get_segment_and_block_offset(parent.direct[0], &blk_seg, &blk_off);

    char block[BLOCK_SIZE];
    fseek(data_segments[blk_seg], blk_off * BLOCK_SIZE, SEEK_SET);
    fread(block, BLOCK_SIZE, 1, data_segments[blk_seg]);

    // Search for file in directory
    uint32_t found_inode = (uint32_t)-1;
    int offset = 0;
    while (offset < BLOCK_SIZE) {
        DirEntry *entry = (DirEntry *)(block + offset);
        if (entry->inode_num == 0 || entry->name_len == 0) break;

        if (entry->name_len == strlen(filename) &&
            strncmp(entry->name, filename, entry->name_len) == 0) {
            found_inode = entry->inode_num;
            break;
        }

        offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
    }

    if (found_inode == (uint32_t)-1) {
        fprintf(stderr, "[extract] File '%s' not found in directory '%s'\n", filename, parent_path);
        return;
    }

    // Load the file inode
    int inode_seg, inode_off;
    get_segment_and_inode_offset(found_inode, &inode_seg, &inode_off);

    Inode file_inode;
    fseek(inode_segments[inode_seg], inode_off * sizeof(Inode), SEEK_SET);
    fread(&file_inode, sizeof(Inode), 1, inode_segments[inode_seg]);

    if (file_inode.type != TYPE_FILE) {
        fprintf(stderr, "[extract] '%s' is not a file\n", filename);
        return;
    }

    uint32_t remaining = file_inode.size;
    uint8_t buffer[BLOCK_SIZE];

    // --- Direct blocks ---
    for (size_t i = 0; i < DIRECT_BLOCKS && remaining > 0; ++i) {
        if (file_inode.direct[i] == 0) break;

        int seg, blk;
        get_segment_and_block_offset(file_inode.direct[i], &seg, &blk);
        uint32_t to_read = (remaining > BLOCK_SIZE) ? BLOCK_SIZE : remaining;

        fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
        fread(buffer, 1, to_read, data_segments[seg]);
        fwrite(buffer, 1, to_read, stdout);
        remaining -= to_read;

        fprintf(stderr, "[extract] Direct block %zu (block=%u) read, %u bytes\n", i, file_inode.direct[i], to_read);
    }

    // --- Single indirect blocks ---
    if (remaining > 0 && file_inode.indirect_single != 0) {
        fprintf(stderr, "[extract] Reading single indirect block: %u\n", file_inode.indirect_single);
        extract_indirect_block(file_inode.indirect_single, &remaining);
    }

    // --- Double indirect blocks ---
    if (remaining > 0 && file_inode.indirect_double != 0) {
        fprintf(stderr, "[extract] Reading double indirect block: %u\n", file_inode.indirect_double);
        uint32_t dbl[PTRS_PER_BLOCK] = {0};
        extract_block_list(file_inode.indirect_double, dbl, PTRS_PER_BLOCK);

        for (size_t i = 0; i < PTRS_PER_BLOCK && remaining > 0; ++i) {
            if (dbl[i] == 0) break;
            fprintf(stderr, "[extract]   -> sub-block %u\n", dbl[i]);
            extract_indirect_block(dbl[i], &remaining);
        }
    }

    // --- Triple indirect blocks skipped ---
    //if (file_inode.indirect_triple != 0) {
    //    fprintf(stderr, "[extract] Triple indirect blocks not supported. Skipping.\n");
    //}

    // Final report
    if (remaining > 0) {
        fprintf(stderr, "[extract-warning] Extraction incomplete: %u bytes remaining\n", remaining);
    } else {
        fprintf(stderr, "[extract] Extraction complete\n");
    }
}
