#include "tesseract_pro.h"
#include <math.h>

TESS_API int32_t tess_estimate_lighting(
    const TessCameraFrame* frame, 
    float* out_ambient_intensity, 
    float out_light_dir[3], 
    float* out_color_temp_kelvin
) {
    if (!frame || !frame->y_plane || !out_ambient_intensity || !out_light_dir || !out_color_temp_kelvin) return -1;

    int w = frame->width;
    int h = frame->height;
    int stride = frame->stride;
    const uint8_t* img = frame->y_plane;

    uint64_t total_lum = 0;
    int grid_x = 10, grid_y = 10;
    int step_x = w / grid_x;
    int step_y = h / grid_y;

    float centroid_x = 0.0f, centroid_y = 0.0f;
    float total_weight = 0.0f;

    for (int y = 0; y < h; y += step_y) {
        for (int x = 0; x < w; x += step_x) {
            uint8_t val = img[y * stride + x];
            total_lum += val;

            float weight = (float)val / 255.0f;
            centroid_x += (float)x * weight;
            centroid_y += (float)y * weight;
            total_weight += weight;
        }
    }

    // 1. Ambient Intensity Normalized (0.0 to 1.0)
    *out_ambient_intensity = (float)total_lum / (float)(grid_x * grid_y * 255);

    // 2. Estimated Directional Light Vector
    if (total_weight > 0.0f) {
        centroid_x /= total_weight;
        centroid_y /= total_weight;

        float norm_x = (centroid_x - (w * 0.5f)) / (w * 0.5f);
        float norm_y = (centroid_y - (h * 0.5f)) / (h * 0.5f);

        out_light_dir[0] = norm_x;
        out_light_dir[1] = -norm_y; // Invert for spatial coordinate system
        out_light_dir[2] = 1.0f - sqrtf(norm_x * norm_x + norm_y * norm_y);
    } else {
        out_light_dir[0] = 0.0f; out_light_dir[1] = 1.0f; out_light_dir[2] = 0.0f;
    }

    // 3. Estimated Color Temperature (Standard baseline approximation)
    *out_color_temp_kelvin = 6500.0f * (*out_ambient_intensity + 0.5f);
    return 0;
}
