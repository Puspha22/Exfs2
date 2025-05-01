#include "exfs2.h"

#define PTRS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))  // Number of uint32_t pointers per block

/**
 * Find a free inode by scanning all inode segments.
 */
int find_free_inode() {
    Inode inode;
    for (int s = 0; s < num_inode_segments; ++s) {
        for (int i = 0; i < INODES_PER_SEGMENT; ++i) {
            fseek(inode_segments[s], i * sizeof(Inode), SEEK_SET);
            fread(&inode, sizeof(Inode), 1, inode_segments[s]);
            if (inode.type == 0) {
                return (s * INODES_PER_SEGMENT) + i;
            }
        }
    }

    create_new_inode_segment();
    fseek(inode_segments[num_inode_segments - 1], 0, SEEK_SET);
    memset(&inode, 0, sizeof(Inode));
    fwrite(&inode, sizeof(Inode), 1, inode_segments[num_inode_segments - 1]);
    fflush(inode_segments[num_inode_segments - 1]);
    return ((num_inode_segments - 1) * INODES_PER_SEGMENT);
}

/**
 * Find a free data block by scanning all data segments or creating a new one.
 */
int find_free_block() {
    char buffer[BLOCK_SIZE];
    for (int s = 0; s < num_data_segments; ++s) {
        for (int b = 1; b < BLOCKS_PER_SEGMENT; ++b) {
            int used = 0;

            for (int i = 0; i < num_inode_segments && !used; ++i) {
                for (int j = 0; j < INODES_PER_SEGMENT; ++j) {
                    Inode inode;
                    fseek(inode_segments[i], j * sizeof(Inode), SEEK_SET);
                    fread(&inode, sizeof(Inode), 1, inode_segments[i]);
                    for (int k = 0; k < DIRECT_BLOCKS; ++k) {
                        int block_num = inode.direct[k];
                        int seg_idx, blk_idx;
                        get_segment_and_block_offset(block_num, &seg_idx, &blk_idx);
                        if (seg_idx == s && blk_idx == b) {
                            used = 1;
                            break;
                        }
                    }
                    if (used) break;
                }
            }

            if (used) continue;

            fseek(data_segments[s], b * BLOCK_SIZE, SEEK_SET);
            fread(buffer, BLOCK_SIZE, 1, data_segments[s]);
            int all_zero = 1;
            for (size_t j = 0; j < BLOCK_SIZE; ++j) {
                if (buffer[j] != 0) {
                    all_zero = 0;
                    break;
                }
            }

            if (all_zero) return s * BLOCKS_PER_SEGMENT + b;
        }
    }

    create_new_data_segment();
    return (num_data_segments - 1) * BLOCKS_PER_SEGMENT + 1;
}

/**
 * Add a host file to ExFS2 under the provided exfs_path.
 */
void run_add(const char *exfs_path, const char *host_path) {
    fprintf(stderr, "[add] Adding '%s' into '%s'\n", host_path, exfs_path);

    const char *filename = strrchr(exfs_path, '/');
    if (!filename || strlen(filename + 1) == 0) {
        fprintf(stderr, "[add] Invalid path: missing filename\n");
        return;
    }
    filename++;

    int parent_inode = find_or_create_path(exfs_path);
    if (parent_inode < 0) {
        fprintf(stderr, "[add] Failed to resolve parent path\n");
        return;
    }

    FILE *src = fopen(host_path, "rb");
    if (!src) {
        perror("[add] Failed to open host file");
        return;
    }

    fseek(src, 0, SEEK_END);
    size_t total_size = ftell(src);
    rewind(src);

    Inode new_file = {0};
    new_file.type = TYPE_FILE;

    size_t written = 0, bytes_read;
    uint32_t total_blocks = 0;
    int last_percent = -1;
    char buffer[BLOCK_SIZE];

    // Allocate indirect block buffers
    uint32_t *indirect_single = calloc(PTRS_PER_BLOCK, sizeof(uint32_t));
    uint32_t *indirect_double = calloc(PTRS_PER_BLOCK, sizeof(uint32_t));
    uint32_t **double_level = calloc(PTRS_PER_BLOCK, sizeof(uint32_t *));

    for (int i = 0; i < PTRS_PER_BLOCK; i++) {
        double_level[i] = calloc(PTRS_PER_BLOCK, sizeof(uint32_t));
    }

    // --- File block writing loop ---
    while ((bytes_read = fread(buffer, 1, BLOCK_SIZE, src)) > 0) {
        int block = find_free_block();
        int seg, blk;
        get_segment_and_block_offset(block, &seg, &blk);
        fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
        fwrite(buffer, bytes_read, 1, data_segments[seg]);
        fflush(data_segments[seg]);
        new_file.size += bytes_read;

        if (total_blocks < DIRECT_BLOCKS) {
            new_file.direct[total_blocks] = block;
        } else if (total_blocks < DIRECT_BLOCKS + PTRS_PER_BLOCK) {
            indirect_single[total_blocks - DIRECT_BLOCKS] = block;
        } else if (total_blocks < DIRECT_BLOCKS + PTRS_PER_BLOCK * (1 + PTRS_PER_BLOCK)) {
            int i = (total_blocks - DIRECT_BLOCKS - PTRS_PER_BLOCK) / PTRS_PER_BLOCK;
            int j = (total_blocks - DIRECT_BLOCKS - PTRS_PER_BLOCK) % PTRS_PER_BLOCK;
            indirect_double[i] = indirect_double[i] ? indirect_double[i] : find_free_block();
            double_level[i][j] = block;
        } else {
            fprintf(stderr, "[add-error] File too large: triple indirect blocks are not supported\n");
            fclose(src);
            return;
        }

        written += bytes_read;
        total_blocks++;
        int percent = (int)((written * 100) / total_size);
        if (percent != last_percent) {
            fprintf(stderr, "\r[add] Progress: %3d%%", percent);
            last_percent = percent;
        }
    }
    fprintf(stderr, "\r[add] Progress: 100%%\n");
    fclose(src);

    // --- Write single indirect ---
    if (total_blocks > DIRECT_BLOCKS) {
        new_file.indirect_single = find_free_block();
        int seg, blk;
        get_segment_and_block_offset(new_file.indirect_single, &seg, &blk);
        fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
        fwrite(indirect_single, sizeof(uint32_t), PTRS_PER_BLOCK, data_segments[seg]);
        fflush(data_segments[seg]);
    }

    // --- Write double indirect ---
    if (total_blocks > DIRECT_BLOCKS + PTRS_PER_BLOCK) {
        new_file.indirect_double = find_free_block();
        int seg, blk;
        get_segment_and_block_offset(new_file.indirect_double, &seg, &blk);
        uint32_t double_ptrs[PTRS_PER_BLOCK] = {0};

        for (int i = 0; i < PTRS_PER_BLOCK && indirect_double[i]; i++) {
            double_ptrs[i] = indirect_double[i];
            int seg2, blk2;
            get_segment_and_block_offset(double_ptrs[i], &seg2, &blk2);
            fseek(data_segments[seg2], blk2 * BLOCK_SIZE, SEEK_SET);
            fwrite(double_level[i], sizeof(uint32_t), PTRS_PER_BLOCK, data_segments[seg2]);
        }

        fseek(data_segments[seg], blk * BLOCK_SIZE, SEEK_SET);
        fwrite(double_ptrs, sizeof(uint32_t), PTRS_PER_BLOCK, data_segments[seg]);
    }

    // --- Write inode ---
    int inode_num = find_free_inode();
    int seg, off;
    get_segment_and_inode_offset(inode_num, &seg, &off);
    fseek(inode_segments[seg], off * sizeof(Inode), SEEK_SET);
    fwrite(&new_file, sizeof(Inode), 1, inode_segments[seg]);
    fflush(inode_segments[seg]);

    // --- Add directory entry ---
    update_directory_entry(parent_inode, inode_num, filename);

    fprintf(stderr, "[add] File '%s' added successfully. size=%u bytes\n", filename, new_file.size);

    // --- Cleanup ---
    free(indirect_single);
    free(indirect_double);
    for (int i = 0; i < PTRS_PER_BLOCK; i++) {
        free(double_level[i]);
    }
    free(double_level);
}
