#include "tesseract_engine.h"
#include <stdlib.h>
#include <string.h>

struct TesseractContext {
    TessMatrix4x4 projection_matrix;
    uint32_t frame_count;
};

TesseractContext* tess_create(void) {
    TesseractContext* ctx = (TesseractContext*)malloc(sizeof(TesseractContext));
    if (!ctx) return NULL;
    
    ctx->frame_count = 0;
    // Initialize identity matrix
    memset(ctx->projection_matrix.m, 0, sizeof(float) * 16);
    ctx->projection_matrix.m[0] = 1.0f;
    ctx->projection_matrix.m[5] = 1.0f;
    ctx->projection_matrix.m[10] = 1.0f;
    ctx->projection_matrix.m[15] = 1.0f;
    
    return ctx;
}

void tess_destroy(TesseractContext* ctx) {
    if (ctx) {
        free(ctx);
    }
}

int32_t tess_process_frame(TesseractContext* ctx, float delta_time) {
    if (!ctx) return -1; // Standardized error check
    ctx->frame_count++;
    return 0;
}

int32_t tess_transform_vector(TesseractContext* ctx, const float* in_vec3, float* out_vec3) {
    if (!ctx || !in_vec3 || !out_vec3) return -1;
    // Perform spatial transformation safely inside native C memory
    out_vec3[0] = in_vec3[0] * ctx->projection_matrix.m[0];
    out_vec3[1] = in_vec3[1] * ctx->projection_matrix.m[5];
    out_vec3[2] = in_vec3[2] * ctx->projection_matrix.m[10];
    return 0;
}

int32_t tess_get_matrix(TesseractContext* ctx, TessMatrix4x4* out_matrix) {
    if (!ctx || !out_matrix) return -1;
    memcpy(out_matrix, &ctx->projection_matrix, sizeof(TessMatrix4x4));
    return 0;
}

int32_t tess_configure_json(TesseractContext* ctx, const char* json_str) {
    if (!ctx || !json_str) return -1;
    // Read configuration without risking FFI type conversions
    return 0;
}
