#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fts.h>
#include <limits.h>
#include <libgen.h>
#include <stdbool.h>

#define PATH_MAX 4096

// Function declarations
static void search_in_dir(const char *dir, const unsigned char *byte_sequence, size_t byte_sequence_length, int *byte_num, bool flag);
unsigned char* convert_hex_string_to_bytes(const char *hex_string);

int main(int argc, char *argv[]) {
    // Check if the correct number of arguments is provided
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Error: Incorrect command format.\n");
        printf("Try to use -h\n");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-h") != 0 && strcmp(argv[1], "--help") != 0 && strcmp(argv[1], "-v") != 0 && strcmp(argv[1], "--version") != 0 && (argc < 3)) {
        fprintf(stderr, "Error: Incorrect command format.\n");
        printf("Try to use -h\n");        
        return EXIT_FAILURE;
    }

    // Display help message if requested
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        printf("Usage: ./a.out [options] `directory` `search_target`\n");
        printf("Options:\n");
        printf("-v, --version\tDisplay program version and executor information.\n");
        printf("-h, --help\tDisplay help information about options.\n");
        return EXIT_SUCCESS;
    }

    // Display version information if requested
    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("Program: find\n");
        printf("Executor: Zakurin Alexandr Nikolaevich\n");
        printf("Версия #неповторимая\n");
        return EXIT_SUCCESS;
    }

    bool debug_flag = 0;
    char *debug_value = getenv("DEBUG");
    if (debug_value != NULL && strcmp(debug_value, "ON") == 0) {
        debug_flag = 1;
    }

    // Extract directory and search target from command-line arguments
    char *dir = argv[1];
    unsigned char *byte_sequence = convert_hex_string_to_bytes(argv[2]);
    if (byte_sequence == NULL) {
        fprintf(stderr, "Error: Invalid byte sequence format.\n");
        return EXIT_FAILURE;
    }
    size_t byte_sequence_length = strlen(argv[2]) / 2;

    // Check if the directory exists and get its absolute path
    char *resolved_path = malloc(PATH_MAX);
    if (resolved_path == NULL) {
        perror("Error allocating memory for resolved path");
        return EXIT_FAILURE;
    }
    if (realpath(dir, resolved_path) == NULL) {
        fprintf(stderr, "Error: Invalid directory path: %s\n", dir);
        printf("Try to use -h\n");
        free(resolved_path);
        free(byte_sequence);
        return EXIT_FAILURE;
    }

    // Fork a new process
    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "Error: Failed to create a new process.\n");
        printf("Try to use -h\n");
        free(resolved_path);
        return EXIT_FAILURE;
    }

    if (pid > 0) {
        // Parent process waits for the child process to finish
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS) {
            fprintf(stderr, "Error in child process.\n");
            free(resolved_path);
            return EXIT_FAILURE;
        }
    } else {
        // Child process performs the directory search
        int byte_num = 0;
        search_in_dir(resolved_path, byte_sequence, byte_sequence_length, &byte_num, debug_flag);
        free(byte_sequence);
        free(resolved_path);
        exit(EXIT_SUCCESS);
    }

    // Free allocated memory
    free(byte_sequence);
    free(resolved_path);

    return EXIT_SUCCESS;
}

// Function to search for a sequence of bytes in files within a directory
void search_in_dir(const char *dir, const unsigned char *byte_sequence, size_t byte_sequence_length, int *byte_num, bool flag) {
    FTS *ftsp;
    FTSENT *ent;

    // Create an array of directories to be searched
    char *dirs[] = {(char *)dir, NULL};

    // Open the directory stream for traversal
    if ((ftsp = fts_open(dirs, FTS_PHYSICAL, NULL)) == NULL) {
        perror("Error opening directory");
        return;
    }

    // Traverse the directory and its subdirectories
    while ((ent = fts_read(ftsp)) != NULL) {
        switch (ent->fts_info) {
            case FTS_F: // Regular file
                // Open the file for reading in binary mode
                FILE *fp = fopen(ent->fts_path, "rb");
                if (fp == NULL) {
                    perror("Error opening file");
                    continue;
                }

                // Initialize byte number and buffer for reading bytes
                *byte_num = 0;
                unsigned char byte;
                size_t n;
                // Read bytes from the file
                while ((n = fread(&byte, sizeof(byte), 1, fp)) == 1) {
                    // Check if the byte matches the first byte of the target sequence
                    if (byte == byte_sequence[0]) {
                        // Initialize flag for matching sequence
                        bool sequence_matched = 1;
                        // Check if the subsequent bytes match the rest of the target sequence
                        for (size_t i = 1; i < byte_sequence_length; ++i) {
                            if (fread(&byte, sizeof(byte), 1, fp) != 1 || byte != byte_sequence[i]) {
                                sequence_matched = 0;
                                break;
                            }
                        }
                        // If the sequence is matched, print the path of the file and byte number
                        if (sequence_matched) {
                            if (flag)
                                printf("%s(DEBUG: LINE NUMBER)  %d\n", ent->fts_path, *byte_num);
                            else 
                                printf("%s\n", ent->fts_path);
                            break;
                        }
                    }
                    ++(*byte_num);
                }

                // Close the file
                fclose(fp);
                break;
            case FTS_D: // Directory
                // Directory entry, no action needed
                break;
            case FTS_DP: // Directory post-order visit
                // Directory post-order visit, no action needed
                break;
            case FTS_DC: // Directory cycle error
                perror("Error while traversing directory");
                break;
            default:
                // Default case, no action needed
                break;
        }
    }

    // Close the directory stream
    fts_close(ftsp);
}

// Function to convert a hexadecimal string to an array of bytes
unsigned char* convert_hex_string_to_bytes(const char *hex_string) {
    size_t len = strlen(hex_string);
    if (len % 2 != 0) {
        // Invalid length for a hexadecimal string
        return NULL;
    }

    size_t byte_count = len / 2;
    unsigned char *bytes = malloc(byte_count);
    if (bytes == NULL) {
        // Memory allocation failed
        return NULL;
    }

    for (size_t i = 0; i < byte_count; ++i) {
        sscanf(hex_string + 2 * i, "%2hhx", &bytes[i]);
    }

    return bytes;
}
