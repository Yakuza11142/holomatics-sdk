#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Platform-agnostic path separator definition */
#if defined(_WIN32) || defined(_WIN64)
    #define PATH_SEP '\\'
#else
    #define PATH_SEP '/'
#endif

/* Forward declaration for the Veloct compilation pipeline */
extern int compile_velo_to_c(const char *input_buffer, size_t input_size, FILE *output_stream);

void print_usage(const char *exec_name) {
    printf("Veloct Compiler Driver (Cross-Platform)\n");
    printf("Usage: %s <source.vlo> [-o output_file]\n", exec_name);
    printf("Options:\n");
    printf("  -o <file>   Specify the output file name (default: output.c)\n");
    printf("  -h, --help  Display this help information\n");
}

int main(int argc, char *argv[]) {
    const char *input_filepath = NULL;
    const char *output_filepath = "output.c";
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    /* Parse CLI arguments cleanly across all OS platforms */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_filepath = argv[++i];
            } else {
                fprintf(stderr, "Error: Flag '-o' requires an output file path.\n");
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown flag '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            input_filepath = argv[i];
        }
    }

    if (!input_filepath) {
        fprintf(stderr, "Error: No input source file specified.\n");
        return 1;
    }

    /* Open input file in binary mode for cross-platform carriage return handling (\r\n vs \n) */
    FILE *input_file = fopen(input_filepath, "rb");
    if (!input_file) {
        fprintf(stderr, "Error: Could not open source file '%s'\n", input_filepath);
        return 1;
    }

    /* Safely compute file size */
    fseek(input_file, 0, SEEK_END);
    long length = ftell(input_file);
    if (length < 0) {
        fprintf(stderr, "Error: Failed to determine file size for '%s'\n", input_filepath);
        fclose(input_file);
        return 1;
    }
    fseek(input_file, 0, SEEK_SET);

    size_t source_size = (size_t)length;
    char *source_buffer = (char *)malloc(source_size + 1);
    if (!source_buffer) {
        fprintf(stderr, "Error: Out of memory while allocating %zu bytes.\n", source_size + 1);
        fclose(input_file);
        return 1;
    }

    size_t read_bytes = fread(source_buffer, 1, source_size, input_file);
    source_buffer[read_bytes] = '\0';
    fclose(input_file);

    /* Open target output file */
    FILE *output_file = fopen(output_filepath, "wb");
    if (!output_file) {
        fprintf(stderr, "Error: Could not open output file '%s' for writing.\n", output_filepath);
        free(source_buffer);
        return 1;
    }

    /* Execute the Veloct compilation core */
    int result = compile_velo_to_c(source_buffer, read_bytes, output_file);

    /* Cleanup memory and handles */
    free(source_buffer);
    fclose(output_file);

    if (result != 0) {
        fprintf(stderr, "Compilation failed with status code %d.\n", result);
        return result;
    }

    printf("Successfully compiled '%s' to '%s'.\n", input_filepath, output_filepath);
    return 0;
}
