// path.c
#include "exfs2.h"

/**
 * Find the last component (filename) and return its pointer,
 * writing the parent path into `parent_out`.
 */
const char* extract_path_tail(const char *exfs_path, char *parent_out) {
    strncpy(parent_out, exfs_path, MAX_PATH);
    parent_out[MAX_PATH - 1] = '\0';
    char *last_slash = strrchr(parent_out, '/');
    if (!last_slash || *(last_slash + 1) == '\0') return NULL;
    const char *filename = last_slash + 1;
    *last_slash = '\0';
    if (strlen(parent_out) == 0) strcpy(parent_out, "/");
    return filename;
}

/**
 * Find the inode number for the given exfs path.
 * Returns -1 if the path is invalid or not found.
 */
int find_inode_by_path(const char *exfs_path) {
    uint32_t current_inode_num = 0;  // Start from root inode (0)

    char path_copy[MAX_PATH];
    strncpy(path_copy, exfs_path, MAX_PATH);
    path_copy[MAX_PATH - 1] = '\0';

    char *tokens[MAX_PATH_DEPTH];
    int depth = 0;
    char *token = strtok(path_copy, "/");
    while (token && depth < MAX_PATH_DEPTH - 1) {
        tokens[depth++] = token;
        token = strtok(NULL, "/");
    }

    for (int i = 0; i < depth; ++i) {
        char *dirname = tokens[i];

        int seg, off;
        get_segment_and_inode_offset(current_inode_num, &seg, &off);

        Inode dir_inode;
        fseek(inode_segments[seg], off * sizeof(Inode), SEEK_SET);
        fread(&dir_inode, sizeof(Inode), 1, inode_segments[seg]);

        if (dir_inode.type != TYPE_DIR) {
            fprintf(stderr, "[path] Inode %u is not a directory\n", current_inode_num);
            return -1;
        }

        int blk_seg, blk_off;
        get_segment_and_block_offset(dir_inode.direct[0], &blk_seg, &blk_off);

        char block[BLOCK_SIZE];
        fseek(data_segments[blk_seg], blk_off * BLOCK_SIZE, SEEK_SET);
        fread(block, BLOCK_SIZE, 1, data_segments[blk_seg]);

        int offset = 0, found = 0;
        while (offset < BLOCK_SIZE) {
            DirEntry *entry = (DirEntry *)(block + offset);
            if (entry->inode_num == 0 || entry->name_len == 0) break;

            if (strncmp(entry->name, dirname, entry->name_len) == 0 &&
                (uint8_t)strlen(dirname) == entry->name_len) {
                current_inode_num = entry->inode_num;
                found = 1;
                break;
            }

            offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
        }

        if (!found) {
            fprintf(stderr, "[path] Directory '%s' not found in path\n", dirname);
            return -1;
        }
    }

    return current_inode_num;
}

/**
 * Recursively prints a directory tree from the given inode.
 */
void print_directory_recursive(uint32_t inode_num, int depth, uint8_t *visited) {
    if (inode_num >= MAX_SEGMENTS * INODES_PER_SEGMENT || visited[inode_num]) return;
    visited[inode_num] = 1;

    int inode_seg, inode_off;
    get_segment_and_inode_offset(inode_num, &inode_seg, &inode_off);
    Inode inode;
    fseek(inode_segments[inode_seg], inode_off * sizeof(Inode), SEEK_SET);
    fread(&inode, sizeof(Inode), 1, inode_segments[inode_seg]);
    if (inode.type != TYPE_DIR) return;

    int blk_seg, blk_off;
    get_segment_and_block_offset(inode.direct[0], &blk_seg, &blk_off);
    char block[BLOCK_SIZE];
    fseek(data_segments[blk_seg], blk_off * BLOCK_SIZE, SEEK_SET);
    fread(block, BLOCK_SIZE, 1, data_segments[blk_seg]);

    int offset = 0;
    while (offset < BLOCK_SIZE) {
        DirEntry *entry = (DirEntry *)(block + offset);
        if (entry->inode_num == 0 || entry->name_len == 0) break;
        for (int i = 0; i < depth; ++i) printf("  ");
        printf("|- %s\n", entry->name);
        print_directory_recursive(entry->inode_num, depth + 1, visited);
        offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
    }
}

/**
 * Top-level list command.
 */
void run_list() {
    fprintf(stderr, "[list] Listing file system contents\n");
    uint8_t visited[MAX_SEGMENTS * INODES_PER_SEGMENT] = {0};
    print_directory_recursive(0, 0, visited);
}

/**
 * Traverses the exfs_path, creating any missing intermediate directories.
 * Returns the parent inode number where the final file/dir will be placed.
 */
int find_or_create_path(const char *exfs_path) {
    uint32_t current_inode_num = 0;  // Always start from root inode 0
    char path_copy[MAX_PATH];
    strncpy(path_copy, exfs_path, MAX_PATH);
    path_copy[MAX_PATH - 1] = '\0';

    char *tokens[MAX_PATH_DEPTH];
    int depth = 0;
    char *token = strtok(path_copy, "/");

    while (token && depth < MAX_PATH_DEPTH - 1) {
        tokens[depth++] = token;
        token = strtok(NULL, "/");
    }

    for (int i = 0; i < depth - 1; ++i) {  // Traverse and create intermediate directories, stopping before final file/dir
        char *dirname = tokens[i];

        int seg, off;
        get_segment_and_inode_offset(current_inode_num, &seg, &off);
        Inode dir_inode;
        fseek(inode_segments[seg], off * sizeof(Inode), SEEK_SET);
        fread(&dir_inode, sizeof(Inode), 1, inode_segments[seg]);

        get_segment_and_block_offset(dir_inode.direct[0], &seg, &off);
        char block[BLOCK_SIZE];
        fseek(data_segments[seg], off * BLOCK_SIZE, SEEK_SET);
        fread(block, BLOCK_SIZE, 1, data_segments[seg]);

        int offset = 0, found = 0;
        uint32_t next_inode = 0;

        while (offset < BLOCK_SIZE) {
            DirEntry *entry = (DirEntry *)(block + offset);
            if (entry->inode_num == 0 || entry->name_len == 0) break;

            if (strncmp(entry->name, dirname, entry->name_len) == 0 &&
                (uint8_t)strlen(dirname) == entry->name_len) {
                next_inode = entry->inode_num;
                found = 1;
                break;
            }

            offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
        }

        if (!found) {
            int new_inode = find_free_inode();
            int new_block = find_free_block();

            Inode new_dir = {0};
            new_dir.type = TYPE_DIR;
            new_dir.direct[0] = new_block;
            fseek(inode_segments[new_inode / INODES_PER_SEGMENT],
                  (new_inode % INODES_PER_SEGMENT) * sizeof(Inode), SEEK_SET);
            fwrite(&new_dir, sizeof(Inode), 1, inode_segments[new_inode / INODES_PER_SEGMENT]);
            fflush(inode_segments[new_inode / INODES_PER_SEGMENT]);

            // Create directory entry
            DirEntry entry = {0};
            entry.inode_num = new_inode;
            entry.name_len = strlen(dirname);
            strncpy(entry.name, dirname, MAX_NAME_LEN);

            int insert_offset = 0;
            while (insert_offset < BLOCK_SIZE) {
                DirEntry *slot = (DirEntry *)(block + insert_offset);
                if (slot->inode_num == 0 || slot->name_len == 0) {
                    memcpy(block + insert_offset, &entry.inode_num, sizeof(uint32_t));
                    memcpy(block + insert_offset + sizeof(uint32_t), &entry.name_len, sizeof(uint8_t));
                    strncpy(block + insert_offset + sizeof(uint32_t) + sizeof(uint8_t),
                            entry.name, entry.name_len + 1);
                    break;
                }
                insert_offset += sizeof(uint32_t) + sizeof(uint8_t) + slot->name_len + 1;
            }

            fseek(data_segments[seg], off * BLOCK_SIZE, SEEK_SET);
            fwrite(block, BLOCK_SIZE, 1, data_segments[seg]);
            fflush(data_segments[seg]);

            next_inode = new_inode;
        }

        current_inode_num = next_inode;
    }

    return current_inode_num;  // Return parent directory inode number
}
