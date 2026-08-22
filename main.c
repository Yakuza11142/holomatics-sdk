#include "velo.h"

void compile_velo_to_c(const char* source, FILE* out_file);

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: veloc <file.vlo> -o <output>\n");
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        printf("Error: Could not open source file %s\n", argv[1]);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* source = malloc(size + 1);
    fread(source, 1, size, file);
    source[size] = '\0';
    fclose(file);

    FILE* out = fopen("build_out.c", "w");
    compile_velo_to_c(source, out);
    fclose(out);

    free(source);
    printf("[VELOC] Compilation complete: Velo -> C Output generated successfully.\n");
    return 0;
}
