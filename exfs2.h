#ifndef EXFS2_H
#define EXFS2_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define BLOCK_SIZE 4096                       // Block size in bytes
#define SEGMENT_SIZE (1024 * 1024)            // Segment size (1MB)
#define MAX_NAME_LEN 255                      // Maximum filename length
#define MAX_SEGMENTS 1024                     // Max number of segments supported
#define INODES_PER_SEGMENT 256                // Number of inodes per inode segment
#define BLOCKS_PER_SEGMENT 256                // Number of blocks per data segment
#define DIRECT_BLOCKS 12                      // Number of direct blocks in inode
#define MAX_PATH 1024                         // Maximum path string length
#define MAX_PATH_DEPTH 64                     // Maximum depth of directory tree
#define PTRS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t)) // Pointers per indirect block

#define TYPE_FILE 1
#define TYPE_DIR  2

// Directory Entry structure (packed to avoid padding)
typedef struct {
    uint32_t inode_num;           // Inode number this entry points to
    uint8_t name_len;             // Length of the filename
    char name[MAX_NAME_LEN + 1];  // Null-terminated filename
} __attribute__((packed)) DirEntry;

// Inode structure (fixed to one block in size)
typedef struct {
    uint32_t size;                        // File size in bytes
    uint16_t type;                        // TYPE_FILE or TYPE_DIR
    uint32_t direct[DIRECT_BLOCKS];       // Direct data block pointers
    uint32_t indirect_single;             // Pointer to single indirect block
    uint32_t indirect_double;             // Pointer to double indirect block
    char padding[BLOCK_SIZE - sizeof(uint32_t) * (DIRECT_BLOCKS + 2) - sizeof(uint16_t)];
} __attribute__((packed)) Inode;

// Global segment file pointers
extern FILE *inode_segments[MAX_SEGMENTS];
extern FILE *data_segments[MAX_SEGMENTS];
extern int num_inode_segments;
extern int num_data_segments;

// Core filesystem utilities
void init_filesystem();
void create_new_inode_segment();
void create_new_data_segment();
int find_free_inode();
int find_free_block();
void get_segment_and_block_offset(int global_block_num, int *segment_idx, int *block_offset);
void get_segment_and_inode_offset(int global_inode_num, int *segment_idx, int *inode_offset);
int find_or_create_path(const char *exfs_path);
int find_inode_by_path(const char *exfs_path);
const char* extract_path_tail(const char *exfs_path, char *parent_out);

// Command implementations
void run_add(const char *exfs_path, const char *host_path);
void run_extract(const char *exfs_path);
void run_remove(const char *exfs_path);
void run_list();
void run_debug(const char *exfs_path);

// Utility for recursive directory listing
void print_directory_recursive(uint32_t inode_num, int depth, uint8_t *visited);

// Block reading utilities
void extract_block_list(uint32_t block_num, uint32_t *out_blocks, size_t max_blocks);
void extract_indirect_block(uint32_t block_num, uint32_t *remaining);

// Directory entry helper
void update_directory_entry(uint32_t parent_inode_num, uint32_t new_inode_num, const char *filename);

#endif // EXFS2_H
