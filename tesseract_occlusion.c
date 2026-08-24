#include "tesseract_pro.h"
#include <string.h>

TESS_API int32_t tess_generate_depth_occlusion_mask(
    const TessCameraIntrinsics* K, 
    const TessVec3* point_cloud, 
    uint32_t point_count, 
    float* depth_buffer, 
    uint32_t width, 
    uint32_t height
) {
    if (!K || !point_cloud || !depth_buffer || width == 0 || height == 0) return -1;

    // Clear Depth Buffer with Maximum Depth (Far Plane)
    for (uint32_t i = 0; i < width * height; i++) {
        depth_buffer[i] = 1e6f;
    }

    // Splat Point Cloud into Depth Buffer
    for (uint32_t i = 0; i < point_count; i++) {
        const TessVec3* p = &point_cloud[i];
        if (p->z <= 0.1f) continue; // Clip Near Plane

        // Pin-hole Projection
        float u_f = (K->fx * p->x / p->z) + K->cx;
        float v_f = (K->fy * p->y / p->z) + K->cy;

        int u = (int)u_f;
        int v = (int)v_f;

        // Splat 3x3 Pixel Radius for Occlusion Mask Solidification
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int px = u + dx;
                int py = v + dy;

                if (px >= 0 && px < (int)width && py >= 0 && py < (int)height) {
                    uint32_t idx = py * width + px;
                    if (p->z < depth_buffer[idx]) {
                        depth_buffer[idx] = p->z;
                    }
                }
            }
        }
    }
    return 0;
}
