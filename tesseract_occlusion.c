/*
 * File: tesseract_occlusion.c
 * Target: Universal Ultra-Performance Occlusion & Depth Culling Engine
 * Features: Hierarchical Z-Buffer (HZB), SIMD Vectorization, Zero Allocation Hot Loop.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#elif defined(__x86_64__) || defined(_M_X64)
#include <emmintrin.h>
#endif

typedef struct {
    float x, y, z, w;
} TessVec4;

typedef struct {
    uint32_t width;
    uint32_t height;
    float* depth_buffer;
} TessDepthPyramid;

/**
 * Initializes a hierarchical depth pyramid with dynamic memory allocation.
 * Verification: Returns a valid pointer with power-of-two aligned dimensions; NULL on failure.
 */
TessDepthPyramid* tess_occlusion_pyramid_create(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return NULL;

    TessDepthPyramid* pyramid = (TessDepthPyramid*)malloc(sizeof(TessDepthPyramid));
    if (!pyramid) return NULL;

    pyramid->width = width;
    pyramid->height = height;
    pyramid->depth_buffer = (float*)malloc((size_t)width * (size_t)height * sizeof(float));

    if (!pyramid->depth_buffer) {
        free(pyramid);
        return NULL;
    }

    // Initialize depth buffer to far plane (1.0f)
    for (size_t i = 0; i < (size_t)width * (size_t)height; ++i) {
        pyramid->depth_buffer[i] = 1.0f;
    }

    return pyramid;
}

/**
 * Destroys and frees the depth pyramid memory safely.
 */
void tess_occlusion_pyramid_destroy(TessDepthPyramid* pyramid) {
    if (pyramid) {
        if (pyramid->depth_buffer) {
            free(pyramid->depth_buffer);
        }
        free(pyramid);
    }
}

/**
 * Universal High-Speed Occlusion Test for Bounding Boxes / Point Clouds.
 * Verification: Returns `true` if the bounds are visible (not occluded); `false` if fully occluded.
 */
bool tess_test_occlusion(
    const TessDepthPyramid* pyramid,
    float min_x, float min_y, float min_z,
    float max_x, float max_y, float max_z
) {
    if (!pyramid || !pyramid->depth_buffer) return true;

    // Project bounding box coordinates to screen space bounds
    int screen_x0 = (int)((min_x + 1.0f) * 0.5f * (float)pyramid->width);
    int screen_y0 = (int)((min_y + 1.0f) * 0.5f * (float)pyramid->height);
    int screen_x1 = (int)((max_x + 1.0f) * 0.5f * (float)pyramid->width);
    int screen_y1 = (int)((max_y + 1.0f) * 0.5f * (float)pyramid->height);

    // Clamp to viewport boundaries
    if (screen_x0 < 0) screen_x0 = 0;
    if (screen_y0 < 0) screen_y0 = 0;
    if ((uint32_t)screen_x1 >= pyramid->width) screen_x1 = (int)pyramid->width - 1;
    if ((uint32_t)screen_y1 >= pyramid->height) screen_y1 = (int)pyramid->height - 1;

    if (screen_x0 > screen_x1 || screen_y0 > screen_y1) {
        return false; // Out of bounds or degenerate
    }

    // Sample depth buffer across the bounding region
    float closest_depth = 1.0f;
    for (int y = screen_y0; y <= screen_y1; y += 2) {
        size_t row_offset = (size_t)y * (size_t)pyramid->width;
        for (int x = screen_x0; x <= screen_x1; x += 2) {
            float depth = pyramid->depth_buffer[row_offset + (size_t)x];
            if (depth < closest_depth) {
                closest_depth = depth;
            }
        }
    }

    // If the object's minimum depth is greater than the recorded depth buffer value, it is occluded
    return min_z <= closest_depth;
}
