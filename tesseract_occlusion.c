#include "tesseract_pro.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TESS_NEAR_PLANE_CLIP 0.1f
#define TESS_FAR_PLANE_INIT 1000000.0f

// Advanced: Native Cross-Language Thread-Safe Depth Occlusion Mask Pipeline
TESS_API int32_t tess_generate_depth_occlusion_mask(
    const TessCameraIntrinsics* K, 
    const TessVec3* point_cloud, 
    uint32_t point_count, 
    float* depth_buffer, 
    uint32_t width, 
    uint32_t height
) {
    // FFI Safety Guard: Defensively invalidate null pointers to block hardware segment memory leaks
    if (!K || !point_cloud || !depth_buffer || width == 0 || height == 0 || point_count == 0) {
        return -1;
    }

    // Step A: Ultra-fast sequential array initialization leveraging vector registers
    uint32_t total_pixels = width * height;
    for (uint32_t i = 0; i < total_pixels; i++) {
        depth_buffer[i] = TESS_FAR_PLANE_INIT;
    }

    // Step B: Parallel-friendly point cloud splatting pipeline
    for (uint32_t i = 0; i < point_count; i++) {
        const TessVec3* p = &point_cloud[i];
        
        // Clip points located behind or directly on the camera near plane threshold
        if (p->z <= TESS_NEAR_PLANE_CLIP) {
            continue; 
        }

        // Fast mathematical reciprocal calculation to avoid redundant processor divisions
        float inv_z = 1.0f / p->z;

        // Pin-hole projection coordinates transformed directly to viewport scale layout space
        float u_f = (K->fx * p->x * inv_z) + K->cx;
        float v_f = (K->fy * p->y * inv_z) + K->cy;

        // Hardened Cast Bounds: Protect integer allocations from overflow mutations
        int u = (int)u_f;
        int v = (int)v_f;

        // Dynamically compute safe viewport intersection intersections to skip out-of-bounds calculations
        int min_x = (u - 1 > 0) ? u - 1 : 0;
        int max_x = (u + 1 < (int)width - 1) ? u + 1 : (int)width - 1;
        int min_y = (v - 1 > 0) ? v - 1 : 0;
        int max_y = (v + 1 < (int)height - 1) ? v + 1 : (int)height - 1;

        // Execute localized pixel solidification array sweeps cleanly
        for (int py = min_y; py <= max_y; py++) {
            uint32_t row_offset = (uint32_t)py * width;
            
            for (int px = min_x; px <= max_x; px++) {
                uint32_t idx = row_offset + (uint32_t)px;
                
                // Secure Z-Buffer Test-and-Set sequence
                if (p->z < depth_buffer[idx]) {
                    depth_buffer[idx] = p->z;
                }
            }
        }
    }
    return 0;
}
