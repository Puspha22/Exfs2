#include "exfs2.h"
#include <stdio.h>
#include <string.h>

/**
 * Print detailed information about a file or directory inode.
 */
void run_debug(const char *exfs_path) {
    fprintf(stderr, "[debug] Debugging '%s'\n", exfs_path);

    // Step 1: Resolve the inode for the given path
    int inode_num = find_inode_by_path(exfs_path);
    if (inode_num < 0) {
        fprintf(stderr, "[debug] Path '%s' not found.\n", exfs_path);
        return;
    }

    // Step 2: Load inode from its segment
    int seg, off;
    get_segment_and_inode_offset(inode_num, &seg, &off);

    Inode inode;
    fseek(inode_segments[seg], off * sizeof(Inode), SEEK_SET);
    fread(&inode, sizeof(Inode), 1, inode_segments[seg]);

    // Step 3: Display inode basic metadata
    printf("Inode %d Info:\n", inode_num);
    printf("  Type : %s\n", inode.type == TYPE_DIR ? "Directory" :
                              inode.type == TYPE_FILE ? "File" : "Unknown");
    printf("  Size : %u bytes\n", inode.size);

    // Step 4: Print direct blocks
    printf("  Direct blocks:\n");
    for (int i = 0; i < DIRECT_BLOCKS; i++) {
        if (inode.direct[i]) {
            printf("    [%d] -> Block %u\n", i, inode.direct[i]);
        }
    }

    // Step 5: Print single indirect block contents
    if (inode.indirect_single != 0) {
        printf("  Single Indirect Block: %u\n", inode.indirect_single);
        uint32_t blocks[PTRS_PER_BLOCK] = {0};
        extract_block_list(inode.indirect_single, blocks, PTRS_PER_BLOCK);
        for (int i = 0; i < PTRS_PER_BLOCK; ++i) {
            if (blocks[i] == 0) break;
            printf("    -> %u\n", blocks[i]);
        }
    }

    // Step 6: Print double indirect block contents
    if (inode.indirect_double != 0) {
        printf("  Double Indirect Block: %u\n", inode.indirect_double);
        uint32_t level1[PTRS_PER_BLOCK] = {0};
        extract_block_list(inode.indirect_double, level1, PTRS_PER_BLOCK);

        for (int i = 0; i < PTRS_PER_BLOCK; ++i) {
            if (level1[i] == 0) break;

            printf("    -> Indirect Block %u\n", level1[i]);
            uint32_t level2[PTRS_PER_BLOCK] = {0};
            extract_block_list(level1[i], level2, PTRS_PER_BLOCK);

            for (int j = 0; j < PTRS_PER_BLOCK; ++j) {
                if (level2[j] == 0) break;
                printf("        -> %u\n", level2[j]);
            }
        }
    }

    // Step 7: If it's a directory, print its entries
    if (inode.type == TYPE_DIR) {
        printf("Directory Entries:\n");

        int blk_seg, blk_off;
        get_segment_and_block_offset(inode.direct[0], &blk_seg, &blk_off);

        char block[BLOCK_SIZE];
        fseek(data_segments[blk_seg], blk_off * BLOCK_SIZE, SEEK_SET);
        fread(block, BLOCK_SIZE, 1, data_segments[blk_seg]);

        int offset = 0;
        while (offset < BLOCK_SIZE) {
            DirEntry *entry = (DirEntry *)(block + offset);
            if (entry->inode_num == 0 || entry->name_len == 0) break;
            printf("  - '%s' (inode %u)\n", entry->name, entry->inode_num);
            offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
        }
    }
}
