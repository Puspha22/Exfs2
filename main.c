// main.c
// Entry point for ExFS2 CLI interface
// Dispatches to appropriate file system operations based on command-line arguments

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exfs2.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s -[a|l|r|e|D] ...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Initialize the filesystem (load or create segment files)
    init_filesystem();

    // Dispatch to appropriate command
    if (strcmp(argv[1], "-a") == 0 && argc == 5 && strcmp(argv[3], "-f") == 0) {
        // Add: ./exfs2 -a <exfs_path> -f <host_path>
        run_add(argv[2], argv[4]);
    } else if (strcmp(argv[1], "-l") == 0) {
        // List all files and directories
        run_list();
    } else if (strcmp(argv[1], "-r") == 0 && argc == 3) {
        // Remove: ./exfs2 -r <exfs_path>
        run_remove(argv[2]);
    } else if (strcmp(argv[1], "-e") == 0 && argc == 3) {
        // Extract: ./exfs2 -e <exfs_path>
        run_extract(argv[2]);
    } else if (strcmp(argv[1], "-D") == 0 && argc == 3) {
        // Debug: ./exfs2 -D <exfs_path>
        run_debug(argv[2]);
    } else {
        // Invalid usage
        fprintf(stderr, "Invalid usage.\n");
        fprintf(stderr, "Valid commands:\n");
        fprintf(stderr, "  %s -a <exfs_path> -f <host_path>   # Add file\n", argv[0]);
        fprintf(stderr, "  %s -e <exfs_path>                  # Extract file\n", argv[0]);
        fprintf(stderr, "  %s -r <exfs_path>                  # Remove file\n", argv[0]);
        fprintf(stderr, "  %s -l                              # List files\n", argv[0]);
        fprintf(stderr, "  %s -D <exfs_path>                  # Debug file or directory\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
