#include "exfs2.h"

// Global segment file pointers and counters
FILE *inode_segments[MAX_SEGMENTS];
FILE *data_segments[MAX_SEGMENTS];
int num_inode_segments = 0;
int num_data_segments = 0;

/**
 * Initialize the filesystem by opening or creating all existing segment files.
 * Sets up the root inode if needed.
 */
void init_filesystem() {
    // Load all existing inode segments
    for (int i = 0; i < MAX_SEGMENTS; ++i) {
        char filename[64];
        snprintf(filename, sizeof(filename), "inode_segment_%d.seg", i);
        FILE *fp = fopen(filename, "r+b");
        if (!fp) break;

        inode_segments[i] = fp;
        num_inode_segments++;
    }

    // If no inode segments found, create segment 0
    if (num_inode_segments == 0) {
        char filename[] = "inode_segment_0.seg";
        inode_segments[0] = fopen(filename, "w+b");
        ftruncate(fileno(inode_segments[0]), SEGMENT_SIZE);
        fprintf(stderr, "[init] Created inode segment: %s\n", filename);
        num_inode_segments = 1;
    }

    // Load all existing data segments
    for (int i = 0; i < MAX_SEGMENTS; ++i) {
        char filename[64];
        snprintf(filename, sizeof(filename), "data_segment_%d.seg", i);
        FILE *fp = fopen(filename, "r+b");
        if (!fp) break;

        data_segments[i] = fp;
        num_data_segments++;
    }

    // If no data segments found, create segment 0
    if (num_data_segments == 0) {
        char filename[] = "data_segment_0.seg";
        data_segments[0] = fopen(filename, "w+b");
        ftruncate(fileno(data_segments[0]), SEGMENT_SIZE);
        fprintf(stderr, "[init] Created data segment: %s\n", filename);

        // Zero out root directory block
        char zero[BLOCK_SIZE] = {0};
        fseek(data_segments[0], 0, SEEK_SET);
        fwrite(zero, BLOCK_SIZE, 1, data_segments[0]);
        fflush(data_segments[0]);

        num_data_segments = 1;
    }

    // Set up root inode if not already initialized
    fseek(inode_segments[0], 0, SEEK_SET);
    Inode root;
    fread(&root, sizeof(Inode), 1, inode_segments[0]);
    if (root.type != TYPE_DIR) {
        memset(&root, 0, sizeof(Inode));
        root.type = TYPE_DIR;
        root.direct[0] = 0;  // Root directory uses block 0
        fseek(inode_segments[0], 0, SEEK_SET);
        fwrite(&root, sizeof(Inode), 1, inode_segments[0]);
        fflush(inode_segments[0]);
        fprintf(stderr, "[init] Created root inode (inode 0)\n");
    }

    fprintf(stderr, "[init] Filesystem initialized with %d inode segments and %d data segments.\n",
            num_inode_segments, num_data_segments);
}

/**
 * Create a new inode segment file and add it to the inode_segments array.
 */
void create_new_inode_segment() {
    if (num_inode_segments >= MAX_SEGMENTS) {
        fprintf(stderr, "[fatal] Maximum number of inode segments reached!\n");
        exit(1);
    }

    char filename[64];
    snprintf(filename, sizeof(filename), "inode_segment_%d.seg", num_inode_segments);
    FILE *fp = fopen(filename, "w+b");
    if (!fp) {
        perror("[error] Failed to create new inode segment");
        exit(1);
    }

    ftruncate(fileno(fp), SEGMENT_SIZE);
    inode_segments[num_inode_segments] = fp;
    fprintf(stderr, "[init] Created new inode segment: %s\n", filename);
    num_inode_segments++;
}

/**
 * Create a new data segment file and add it to the data_segments array.
 */
void create_new_data_segment() {
    if (num_data_segments >= MAX_SEGMENTS) {
        fprintf(stderr, "[fatal] Maximum number of data segments reached!\n");
        exit(1);
    }

    char filename[64];
    snprintf(filename, sizeof(filename), "data_segment_%d.seg", num_data_segments);
    FILE *fp = fopen(filename, "w+b");
    if (!fp) {
        perror("[error] Failed to create new data segment");
        exit(1);
    }

    ftruncate(fileno(fp), SEGMENT_SIZE);
    data_segments[num_data_segments] = fp;
    fprintf(stderr, "[init] Created new data segment: %s\n", filename);
    num_data_segments++;
}

/**
 * Maps a global inode number to a segment index and inode index within the segment.
 */
void get_segment_and_inode_offset(int global_inode_num, int *segment_idx, int *inode_offset) {
    *segment_idx = global_inode_num / INODES_PER_SEGMENT;
    *inode_offset = global_inode_num % INODES_PER_SEGMENT;

    if (*segment_idx >= num_inode_segments) {
        fprintf(stderr, "[helpers] Invalid inode segment index %d for inode %d\n",
                *segment_idx, global_inode_num);
        exit(EXIT_FAILURE);
    }
}
