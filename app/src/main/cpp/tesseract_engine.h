#ifndef TESSERACT_ENGINE_H
#define TESSERACT_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TESS_API
#define TESS_API __attribute__((visibility("default")))
#endif

// Fundamental 3D Linear Algebra Structures for Interop Matching
typedef struct {
    float m[16];
} TessMatrix4x4;

typedef struct {
    float x;
    float y;
    float z;
} TessVec3;

typedef struct {
    float fx, fy;
    float cx, cy;
} TessCameraIntrinsics;

// Core Bare-Metal Runtime Pipeline Lifecycle Functions
TESS_API void* tess_create(void);
TESS_API void tess_destroy(void* ctx);
TESS_API int32_t tess_process_frame(void* ctx, float delta_time);
TESS_API int32_t tess_transform_vector(void* ctx, const float* in_vec, float* out_vec);

// Multi-Lane Accelerated Computer Vision Subsystems
TESS_API int32_t tess_generate_depth_occlusion_mask(
    const TessCameraIntrinsics* K,
    const TessVec3* point_cloud,
    uint32_t point_count,
    float* depth_buffer,
    uint32_t width,
    uint32_t height
);

#ifdef __cplusplus
}
#endif

#endif // TESSERACT_ENGINE_H
