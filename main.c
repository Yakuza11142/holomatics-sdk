#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "velo.h"

void compile_velo_to_c(const char* source, FILE* out_file);

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: veloc <file.vlo> [-o output.c]\n");
        return 1;
    }

    const char* input_filename = argv[1];
    const char* output_filename = "build_out.c"; // Default fallback

    // Dynamic output flag parsing
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && (i + 1) < argc) {
            output_filename = argv[i + 1];
            i++;
        }
    }

    // Open source file
    FILE* file = fopen(input_filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open source file '%s'\n", input_filename);
        return 1;
    }

    // Measure file size safely
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fprintf(stderr, "Error: Failed to determine size of '%s'\n", input_filename);
        fclose(file);
        return 1;
    }

    char* source = (char*)malloc(size + 1);
    if (!source) {
        fprintf(stderr, "Error: Memory allocation failed for source buffer (%ld bytes)\n", size + 1);
        fclose(file);
        return 1;
    }

    size_t bytes_read = fread(source, 1, size, file);
    source[bytes_read] = '\0';
    fclose(file);

    // Open target output file
    FILE* out = fopen(output_filename, "w");
    if (!out) {
        fprintf(stderr, "Error: Could not open output target '%s'\n", output_filename);
        free(source);
        return 1;
    }

    // Trigger translation pipeline
    compile_velo_to_c(source, out);

    fclose(out);
    free(source);

    printf("[VELOC] Compilation complete: '%s' -> '%s' generated successfully.\n", 
           input_filename, output_filename);

    return 0;
}
