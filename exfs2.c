#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define BLOCK_SIZE 4096
#define SEGMENT_SIZE (1024 * 1024)
#define MAX_NAME_LEN 255
#define MAX_SEGMENTS 1024
#define INODES_PER_SEGMENT 256
#define BLOCKS_PER_SEGMENT 255
#define DIRECT_BLOCKS 12
#define MAX_PATH 1024
#define MAX_PATH_DEPTH 64

#define TYPE_FILE 1
#define TYPE_DIR  2

// --- Structs for Directory Entry and Inode ---

typedef struct {
    uint32_t inode_num;                  // Inode number of the file/directory
    uint8_t name_len;                    // Length of the file name
    char name[MAX_NAME_LEN + 1];          // File or directory name
} __attribute__((packed)) DirEntry;

typedef struct {
    uint32_t size;                       // File size in bytes (for files)
    uint16_t type;                       // TYPE_FILE or TYPE_DIR
    uint32_t direct[DIRECT_BLOCKS];       // Direct block pointers
    uint32_t indirect_single;             // Not used yet (future extension)
    uint32_t indirect_double;
    uint32_t indirect_triple;
    char padding[BLOCK_SIZE - sizeof(uint32_t)*(DIRECT_BLOCKS + 4) - sizeof(uint16_t)];
} __attribute__((packed)) Inode;

// --- Globals ---

FILE *inode_segments[MAX_SEGMENTS];
FILE *data_segments[MAX_SEGMENTS];
int num_inode_segments = 0;
int num_data_segments = 0;

// --- Function Prototypes ---

void init_filesystem();
int find_free_inode();
int find_free_block();
int find_or_create_path(const char *exfs_path);
void run_add(const char *exfs_path, const char *host_path);
void run_list();
void run_extract(const char *exfs_path);
void run_remove(const char *exfs_path);
void run_debug(const char *exfs_path);
int find_inode_by_path(const char *exfs_path);
const char* extract_path_tail(const char *exfs_path, char *parent_out);
void print_directory_recursive(uint32_t inode_num, int depth, uint8_t *visited);


// Initializes the filesystem by opening (or creating) segment files.
// Ensures inode_segment_0.seg and data_segment_0.seg exist and are formatted.
void init_filesystem() {
    char inode_file[] = "inode_segment_0.seg";
    inode_segments[0] = fopen(inode_file, "r+b");
    if (!inode_segments[0]) {
        inode_segments[0] = fopen(inode_file, "w+b");
        ftruncate(fileno(inode_segments[0]), SEGMENT_SIZE);
        fprintf(stderr, "[init] Created inode segment: %s\n", inode_file);
    }
    num_inode_segments = 1;

    char data_file[] = "data_segment_0.seg";
    data_segments[0] = fopen(data_file, "r+b");
    if (!data_segments[0]) {
        data_segments[0] = fopen(data_file, "w+b");
        ftruncate(fileno(data_segments[0]), SEGMENT_SIZE);
        fprintf(stderr, "[init] Created data segment: %s\n", data_file);

        // Initialize root directory block with zeroes
        char zero[BLOCK_SIZE] = {0};
        fseek(data_segments[0], 0, SEEK_SET);
        fwrite(zero, BLOCK_SIZE, 1, data_segments[0]);
        fflush(data_segments[0]);
    }
    num_data_segments = 1;

    // Initialize root inode (inode 0) if not already
    fseek(inode_segments[0], 0, SEEK_SET);
    Inode root;
    fread(&root, sizeof(Inode), 1, inode_segments[0]);
    if (root.type != TYPE_DIR) {
        memset(&root, 0, sizeof(Inode));
        root.type = TYPE_DIR;
        root.direct[0] = 0; // Root uses block 0
        fseek(inode_segments[0], 0, SEEK_SET);
        fwrite(&root, sizeof(Inode), 1, inode_segments[0]);
        fflush(inode_segments[0]);
        fprintf(stderr, "[init] Created root inode (inode 0)\n");
    }

    fprintf(stderr, "[init] Filesystem initialized.\n");
}


// Finds a free inode by scanning the inode segment for type == 0 (unused).
int find_free_inode() {
    Inode inode;
    for (int i = 1; i < INODES_PER_SEGMENT; i++) {  // Start after root inode
        fseek(inode_segments[0], i * sizeof(Inode), SEEK_SET);
        fread(&inode, sizeof(Inode), 1, inode_segments[0]);
        if (inode.type == 0) {
            return i;
        }
    }
    return -1; // No free inode found
}


// Finds a free block by scanning all inodes to check block usage.
int find_free_block() {
    Inode inode;
    char buffer[BLOCK_SIZE];

    for (uint32_t b = 1; b < BLOCKS_PER_SEGMENT; ++b) {  // <== FIXED signedness warning here
        int used = 0;

        // Check if any inode points to this block
        for (int i = 0; i < INODES_PER_SEGMENT; ++i) {
            fseek(inode_segments[0], i * sizeof(Inode), SEEK_SET);
            fread(&inode, sizeof(Inode), 1, inode_segments[0]);

            for (int j = 0; j < DIRECT_BLOCKS; ++j) {
                if (inode.direct[j] == b) {
                    used = 1;
                    break;
                }
            }
            if (used) break;
        }

        if (!used) {
            // Confirm block is zeroed out
            fseek(data_segments[0], b * BLOCK_SIZE, SEEK_SET);
            fread(buffer, BLOCK_SIZE, 1, data_segments[0]);

            for (int j = 0; j < BLOCK_SIZE; ++j) {
                if (buffer[j] != 0) {
                    used = 1;
                    break;
                }
            }

            if (!used) return b;
        }
    }

    return -1; // No free block found
}


// Traverses the exfs_path, creating any missing intermediate directories.
// Returns the parent inode number where the final file/dir will be placed.
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

    for (int i = 0; i < depth - 1; ++i) {  // Stop at parent of final
        char *dirname = tokens[i];
        fprintf(stderr, "[path] Resolving level %d: %s\n", i, dirname);

        Inode dir_inode;
        fseek(inode_segments[0], current_inode_num * sizeof(Inode), SEEK_SET);
        fread(&dir_inode, sizeof(Inode), 1, inode_segments[0]);

        char block[BLOCK_SIZE];
        fseek(data_segments[0], dir_inode.direct[0] * BLOCK_SIZE, SEEK_SET);
        fread(block, BLOCK_SIZE, 1, data_segments[0]);

        int offset = 0, found = 0;
        uint32_t next_inode = 0;

        while (offset < BLOCK_SIZE) {
            DirEntry *entry = (DirEntry *)(block + offset);
            if (entry->inode_num == 0 || entry->name_len == 0) break;

            if (strncmp(entry->name, dirname, entry->name_len) == 0 &&
                strlen(dirname) == entry->name_len) {
                next_inode = entry->inode_num;
                found = 1;
                fprintf(stderr, "[path] Found existing dir '%s' at inode %u\n", dirname, next_inode);
                break;
            }
            offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
        }

        if (!found) {
            // Directory not found — create it
            int new_inode = find_free_inode();
            int new_block = find_free_block();
            if (new_inode == -1 || new_block == -1) {
                fprintf(stderr, "[path] No space to create '%s'\n", dirname);
                return -1;
            }

            fprintf(stderr, "[path] Creating new dir '%s' at inode %d, block %d\n", dirname, new_inode, new_block);

            Inode new_dir = {0};
            new_dir.type = TYPE_DIR;
            new_dir.direct[0] = new_block;
            fseek(inode_segments[0], new_inode * sizeof(Inode), SEEK_SET);
            fwrite(&new_dir, sizeof(Inode), 1, inode_segments[0]);
            fflush(inode_segments[0]);

            // Insert new directory entry
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
                    strncpy(block + insert_offset + sizeof(uint32_t) + sizeof(uint8_t), entry.name, entry.name_len + 1);
                    fprintf(stderr, "[path] Inserted dir '%s' at offset %d\n", dirname, insert_offset);
                    break;
                }
                insert_offset += sizeof(uint32_t) + sizeof(uint8_t) + slot->name_len + 1;
            }

            // Write updated parent block
            fseek(data_segments[0], dir_inode.direct[0] * BLOCK_SIZE, SEEK_SET);
            fwrite(block, BLOCK_SIZE, 1, data_segments[0]);
            fflush(data_segments[0]);

            next_inode = new_inode;
        }

        current_inode_num = next_inode;
    }

    return current_inode_num;  // return parent inode number
}


// Adds a host file into the filesystem at the specified exfs_path.
void run_add(const char *exfs_path, const char *host_path) {
    fprintf(stderr, "[add] Adding '%s' into '%s'\n", host_path, exfs_path);

    const char *filename = strrchr(exfs_path, '/');
    if (!filename || strlen(filename + 1) == 0) {
        fprintf(stderr, "[add] Invalid path: missing filename\n");
        return;
    }
    filename++;  // Skip the '/'

    int parent_inode_num = find_or_create_path(exfs_path);
    if (parent_inode_num < 0) {
        fprintf(stderr, "[add] Failed to resolve parent path\n");
        return;
    }

    FILE *src = fopen(host_path, "rb");
    if (!src) {
        perror("[add] Failed to open host file");
        return;
    }

    char buffer[BLOCK_SIZE] = {0};
    size_t file_size = fread(buffer, 1, BLOCK_SIZE, src);
    fclose(src);

    // Load parent inode
    Inode parent;
    fseek(inode_segments[0], parent_inode_num * sizeof(Inode), SEEK_SET);
    fread(&parent, sizeof(Inode), 1, inode_segments[0]);

    // Load parent directory block
    char block[BLOCK_SIZE];
    fseek(data_segments[0], parent.direct[0] * BLOCK_SIZE, SEEK_SET);
    fread(block, BLOCK_SIZE, 1, data_segments[0]);

    // Check for duplicates
    int offset = 0;
    while (offset < BLOCK_SIZE) {
        DirEntry *entry = (DirEntry *)(block + offset);
        if (entry->inode_num == 0 || entry->name_len == 0) break;
        if (entry->name_len == strlen(filename) &&
            strncmp(entry->name, filename, entry->name_len) == 0) {
            fprintf(stderr, "[add] File '%s' already exists. Skipping.\n", filename);
            return;
        }
        offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
    }

    // Allocate inode and block
    int new_inode = find_free_inode();
    int new_block = find_free_block();
    if (new_inode < 0 || new_block < 0) {
        fprintf(stderr, "[add] No space left to allocate file.\n");
        return;
    }

    // Write file data
    fseek(data_segments[0], new_block * BLOCK_SIZE, SEEK_SET);
    fwrite(buffer, file_size, 1, data_segments[0]);
    fflush(data_segments[0]);

    // Setup new inode
    Inode new_file = {0};
    new_file.type = TYPE_FILE;
    new_file.size = file_size;
    new_file.direct[0] = new_block;
    fseek(inode_segments[0], new_inode * sizeof(Inode), SEEK_SET);
    fwrite(&new_file, sizeof(Inode), 1, inode_segments[0]);
    fflush(inode_segments[0]);

    // Insert directory entry
    DirEntry new_entry = {0};
    new_entry.inode_num = new_inode;
    new_entry.name_len = strlen(filename);
    strncpy(new_entry.name, filename, MAX_NAME_LEN);

    fprintf(stderr, "[debug] Writing directory entry at offset %d\n", offset);
    memcpy(block + offset, &new_entry.inode_num, sizeof(uint32_t));
    memcpy(block + offset + sizeof(uint32_t), &new_entry.name_len, sizeof(uint8_t));
    strncpy(block + offset + sizeof(uint32_t) + sizeof(uint8_t), new_entry.name, new_entry.name_len + 1);

    // Save parent directory block
    fseek(data_segments[0], parent.direct[0] * BLOCK_SIZE, SEEK_SET);
    fwrite(block, BLOCK_SIZE, 1, data_segments[0]);
    fflush(data_segments[0]);

    fprintf(stderr, "[add] File '%s' added at inode %d, data block %d, size %zu bytes.\n",
            filename, new_inode, new_block, file_size);
}


// Recursively prints the filesystem tree from a starting inode (usually root).
void print_directory_recursive(uint32_t inode_num, int depth, uint8_t *visited) {
    if (inode_num >= INODES_PER_SEGMENT || visited[inode_num]) {
        fprintf(stderr, "[list-debug] Skipping inode %u (already visited)\n", inode_num);
        return;
    }

    visited[inode_num] = 1;  // Mark inode as visited to avoid infinite loops

    Inode dir_inode;
    fseek(inode_segments[0], inode_num * sizeof(Inode), SEEK_SET);
    fread(&dir_inode, sizeof(Inode), 1, inode_segments[0]);

    if (dir_inode.type != TYPE_DIR || dir_inode.direct[0] >= BLOCKS_PER_SEGMENT)
        return;

    char block[BLOCK_SIZE];
    fseek(data_segments[0], dir_inode.direct[0] * BLOCK_SIZE, SEEK_SET);
    fread(block, BLOCK_SIZE, 1, data_segments[0]);

    int offset = 0;
    while (offset < BLOCK_SIZE) {
        DirEntry *entry = (DirEntry *)(block + offset);
        if (entry->inode_num == 0 || entry->name_len == 0) break;

        // Indent according to depth level
        for (int i = 0; i < depth; ++i) printf("   ");
        printf("|- %s\n", entry->name);

        Inode child;
        fseek(inode_segments[0], entry->inode_num * sizeof(Inode), SEEK_SET);
        fread(&child, sizeof(Inode), 1, inode_segments[0]);

        // If entry is a directory, recurse
        if (child.type == TYPE_DIR) {
            print_directory_recursive(entry->inode_num, depth + 1, visited);
        }

        offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
    }
}


// Top-level command to list all filesystem contents recursively from root.
void run_list() {
    fprintf(stderr, "[list] Listing file system contents\n");
    uint8_t visited[INODES_PER_SEGMENT] = {0};  // Array to prevent revisiting inodes
    print_directory_recursive(0, 0, visited);   // Start from inode 0 (root)
}


// Extracts the filename from a full exfs_path.
// Also writes the parent directory path into parent_out.
const char* extract_path_tail(const char *exfs_path, char *parent_out) {
    strncpy(parent_out, exfs_path, MAX_PATH);
    parent_out[MAX_PATH - 1] = '\0';

    char *last_slash = strrchr(parent_out, '/');
    if (!last_slash || *(last_slash + 1) == '\0') {
        return NULL; // Invalid: no filename after slash
    }

    const char *filename = last_slash + 1;
    *last_slash = '\0'; // Terminate parent path at last slash

    if (strlen(parent_out) == 0)
        strcpy(parent_out, "/"); // Special case for root

    return filename;
}


// Traverses a path like /a/b/c and returns the inode number.
// Returns -1 if not found.
int find_inode_by_path(const char *exfs_path) {
    uint32_t current_inode_num = 0;  // Start at root
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

        Inode dir_inode;
        fseek(inode_segments[0], current_inode_num * sizeof(Inode), SEEK_SET);
        fread(&dir_inode, sizeof(Inode), 1, inode_segments[0]);

        if (dir_inode.type != TYPE_DIR) {
            fprintf(stderr, "[path] Inode %u is not a directory\n", current_inode_num);
            return -1;
        }

        char block[BLOCK_SIZE];
        fseek(data_segments[0], dir_inode.direct[0] * BLOCK_SIZE, SEEK_SET);
        fread(block, BLOCK_SIZE, 1, data_segments[0]);

        int offset = 0, found = 0;
        while (offset < BLOCK_SIZE) {
            DirEntry *entry = (DirEntry *)(block + offset);
            if (entry->inode_num == 0 || entry->name_len == 0) break;

            if (strncmp(entry->name, dirname, entry->name_len) == 0 &&
                strlen(dirname) == entry->name_len) {
                current_inode_num = entry->inode_num;
                found = 1;
                break;
            }

            offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
        }

        if (!found) {
            fprintf(stderr, "[path] Directory '%s' not found\n", dirname);
            return -1;
        }
    }

    return current_inode_num;  // Found
}


// Extracts a file from the virtual filesystem to stdout (used with > redirection).
void run_extract(const char *exfs_path) {
    fprintf(stderr, "[extract] Extracting '%s'\n", exfs_path);

    char parent_path[MAX_PATH];
    const char *filename = extract_path_tail(exfs_path, parent_path);
    if (!filename || strlen(filename) == 0) {
        fprintf(stderr, "[extract] Invalid path (missing filename)\n");
        return;
    }

    int parent_inode = find_inode_by_path(parent_path[0] ? parent_path : "/");
    if (parent_inode < 0) {
        fprintf(stderr, "[extract] Parent directory '%s' not found\n", parent_path);
        return;
    }

    Inode parent;
    fseek(inode_segments[0], parent_inode * sizeof(Inode), SEEK_SET);
    fread(&parent, sizeof(Inode), 1, inode_segments[0]);

    char block[BLOCK_SIZE];
    fseek(data_segments[0], parent.direct[0] * BLOCK_SIZE, SEEK_SET);
    fread(block, BLOCK_SIZE, 1, data_segments[0]);

    fprintf(stderr, "[extract-debug] Directory block scan in '%s' looking for '%s':\n", parent_path, filename);

    uint32_t found_inode = (uint32_t)-1;
    int offset = 0;
    while (offset < BLOCK_SIZE) {
        DirEntry *entry = (DirEntry *)(block + offset);
        if (entry->inode_num == 0 || entry->name_len == 0) break;

        fprintf(stderr, "  Entry: inode=%u, name_len=%u, name='%.*s'\n",
                entry->inode_num, entry->name_len, entry->name_len, entry->name);

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

    Inode file_inode;
    fseek(inode_segments[0], found_inode * sizeof(Inode), SEEK_SET);
    fread(&file_inode, sizeof(Inode), 1, inode_segments[0]);

    if (file_inode.type != TYPE_FILE) {
        fprintf(stderr, "[extract] '%s' is not a regular file\n", filename);
        return;
    }

    char file_data[BLOCK_SIZE] = {0};
    fseek(data_segments[0], file_inode.direct[0] * BLOCK_SIZE, SEEK_SET);
    fread(file_data, 1, file_inode.size, data_segments[0]);

    fprintf(stderr, "[extract-debug] Extracting %u bytes from block %u (inode %u)\n",
            file_inode.size, file_inode.direct[0], found_inode);

    // Actual file output to stdout (for redirection)
    fwrite(file_data, 1, file_inode.size, stdout);
}


// Removes a file from the filesystem, clearing its inode and block.
void run_remove(const char *exfs_path) {
    fprintf(stderr, "[remove] Removing '%s'\n", exfs_path);

    char path_copy[MAX_PATH];
    strncpy(path_copy, exfs_path, MAX_PATH);
    path_copy[MAX_PATH - 1] = '\0';

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash || *(last_slash + 1) == '\0') {
        fprintf(stderr, "[remove] Invalid path: %s\n", exfs_path);
        return;
    }

    char *filename = last_slash + 1;
    *last_slash = '\0'; // cut to parent path

    int parent_inode = find_inode_by_path(path_copy[0] ? path_copy : "/");
    if (parent_inode < 0) {
        fprintf(stderr, "[remove] Parent directory not found\n");
        return;
    }

    Inode parent;
    fseek(inode_segments[0], parent_inode * sizeof(Inode), SEEK_SET);
    fread(&parent, sizeof(Inode), 1, inode_segments[0]);

    char block[BLOCK_SIZE];
    fseek(data_segments[0], parent.direct[0] * BLOCK_SIZE, SEEK_SET);
    fread(block, BLOCK_SIZE, 1, data_segments[0]);

    int offset = 0, found_offset = -1;
    uint32_t target_inode = (uint32_t)-1;
    while (offset < BLOCK_SIZE) {
        DirEntry *entry = (DirEntry *)(block + offset);
        if (entry->inode_num == 0 || entry->name_len == 0) break;

        if (strncmp(entry->name, filename, entry->name_len) == 0 &&
            strlen(filename) == entry->name_len) {
            target_inode = entry->inode_num;
            found_offset = offset;
            break;
        }

        offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
    }

    if (target_inode == (uint32_t)-1 || found_offset == -1) {
        fprintf(stderr, "[remove] File not found in directory\n");
        return;
    }

    memset(block + found_offset, 0, sizeof(DirEntry));
    fseek(data_segments[0], parent.direct[0] * BLOCK_SIZE, SEEK_SET);
    fwrite(block, BLOCK_SIZE, 1, data_segments[0]);
    fflush(data_segments[0]);

    Inode target;
    fseek(inode_segments[0], target_inode * sizeof(Inode), SEEK_SET);
    fread(&target, sizeof(Inode), 1, inode_segments[0]);

    if (target.direct[0] < BLOCKS_PER_SEGMENT) {
        char zero_block[BLOCK_SIZE] = {0};
        fseek(data_segments[0], target.direct[0] * BLOCK_SIZE, SEEK_SET);
        fwrite(zero_block, BLOCK_SIZE, 1, data_segments[0]);
        fflush(data_segments[0]);
    }

    Inode empty = {0};
    fseek(inode_segments[0], target_inode * sizeof(Inode), SEEK_SET);
    fwrite(&empty, sizeof(Inode), 1, inode_segments[0]);
    fflush(inode_segments[0]);

    fprintf(stderr, "[remove] File '%s' removed successfully.\n", filename);
}


// Debugs a given path by printing inode details and directory entries (if directory).
void run_debug(const char *exfs_path) {
    fprintf(stderr, "[debug] Debugging '%s'\n", exfs_path);

    int inode_num = find_inode_by_path(exfs_path);
    if (inode_num < 0) {
        fprintf(stderr, "[debug] Path '%s' not found.\n", exfs_path);
        return;
    }

    Inode inode;
    fseek(inode_segments[0], inode_num * sizeof(Inode), SEEK_SET);
    fread(&inode, sizeof(Inode), 1, inode_segments[0]);

    printf("Inode %d Info:\n", inode_num);
    printf("  Type : %s\n", (inode.type == TYPE_DIR) ? "Directory" : (inode.type == TYPE_FILE) ? "File" : "Unknown");
    printf("  Size : %u bytes\n", inode.size);

    printf("  Direct blocks:\n");
    for (int i = 0; i < DIRECT_BLOCKS; i++) {
        if (inode.direct[i]) {
            printf("    [%d] -> Block %u\n", i, inode.direct[i]);
        }
    }

    if (inode.type == TYPE_DIR) {
        printf("Directory Entries:\n");

        char block[BLOCK_SIZE];
        fseek(data_segments[0], inode.direct[0] * BLOCK_SIZE, SEEK_SET);
        fread(block, BLOCK_SIZE, 1, data_segments[0]);

        int offset = 0;
        while (offset < BLOCK_SIZE) {
            DirEntry *entry = (DirEntry *)(block + offset);
            if (entry->inode_num == 0 || entry->name_len == 0) break;

            printf("  - '%s' (inode %u)\n", entry->name, entry->inode_num);

            offset += sizeof(uint32_t) + sizeof(uint8_t) + entry->name_len + 1;
        }
    }
}


// Entry point: Parses command-line arguments and dispatches to correct function.
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s -[a|l|r|e|D] ...\n", argv[0]);
        exit(1);
    }

    init_filesystem();

    if (strcmp(argv[1], "-a") == 0 && argc == 5 && strcmp(argv[3], "-f") == 0) {
        run_add(argv[2], argv[4]);
    } else if (strcmp(argv[1], "-l") == 0) {
        run_list();
    } else if (strcmp(argv[1], "-r") == 0 && argc == 3) {
        run_remove(argv[2]);
    } else if (strcmp(argv[1], "-e") == 0 && argc == 3) {
        run_extract(argv[2]);
    } else if (strcmp(argv[1], "-D") == 0 && argc == 3) {
        run_debug(argv[2]);
    } else {
        fprintf(stderr, "Invalid usage.\n");
        exit(1);
    }

    return 0;
}
