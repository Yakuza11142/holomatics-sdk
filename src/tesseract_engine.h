#ifndef TESSERACT_ENGINE_H
#define TESSERACT_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque engine instance handle (prevents target memory corruption)
typedef struct TesseractContext TesseractContext;

// Matrix structure aligned for GPU vector math with safety guard
#ifndef TESS_MATRIX4X4_DEFINED
#define TESS_MATRIX4X4_DEFINED
typedef struct {
    float m[16];
} TessMatrix4x4;
#endif

#ifndef TESS_RENDER_COMMAND_DEFINED
#define TESS_RENDER_COMMAND_DEFINED
typedef struct {
    uint32_t command_id;
    float delta_time;
    TessMatrix4x4 payload_matrix;
} TessRenderCommand;
#endif

// Engine Initialization & Lifecycle
TesseractContext* tess_create(void);
void tess_destroy(TesseractContext* ctx);

// Core Processing APIs
int32_t tess_process_frame(TesseractContext* ctx, float delta_time);
int32_t tess_transform_vector(TesseractContext* ctx, const float* in_vec3, float* out_vec3);
int32_t tess_get_matrix(TesseractContext* ctx, TessMatrix4x4* out_matrix);

// Memory Safe String Messaging (Passes raw JSON string into engine)
int32_t tess_configure_json(TesseractContext* ctx, const char* json_str);

#ifdef __cplusplus
}
#endif

#endif // TESSERACT_ENGINE_H
