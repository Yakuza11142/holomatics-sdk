#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

struct TessCameraIntrinsics {
    float fx;
    float fy;
    float cx;
    float cy;
};

struct PipelineUniforms {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t total_workgroups;
};

struct Vector3 {
    float x, y, z;
};

struct LightingOutput {
    float ambient_intensity;
    float color_temp_kelvin;
    struct Vector3 light_dir;
};

struct GlobalAtomicAccumulators {
    _Atomic uint32_t global_lum_bits;
    _Atomic uint32_t global_cx_bits;
    _Atomic uint32_t global_cy_bits;
    _Atomic uint32_t workgroups_finished;
};

struct ThreadArg {
    uint32_t gx;
    uint32_t gy;
    struct PipelineUniforms cfg;
    const uint32_t* y_plane_buffer;
    size_t y_plane_size;
    struct GlobalAtomicAccumulators* global_atomics;
    struct LightingOutput* output_data;
    uint32_t total_groups;
};

static void* worker_routine(void* arg) {
    struct ThreadArg* targs = (struct ThreadArg*)arg;
    uint32_t gx = targs->gx;
    uint32_t gy = targs->gy;
    struct PipelineUniforms cfg = targs->cfg;
    const uint32_t* y_plane_buffer = targs->y_plane_buffer;
    struct GlobalAtomicAccumulators* global_atomics = targs->global_atomics;
    struct LightingOutput* output_data = targs->output_data;
    uint32_t total_groups = targs->total_groups;

    const uint32_t WG_SIZE_X = 16;
    const uint32_t WG_SIZE_Y = 16;
    const uint32_t LOCAL_SIZE = WG_SIZE_X * WG_SIZE_Y; // 256

    float s_lum[256] = {0.0f};
    float s_cx[256] = {0.0f};
    float s_cy[256] = {0.0f};

    // Phase 1: Load and compute local thread data across the 16x16 block
    for (uint32_t local_idx = 0; local_idx < LOCAL_SIZE; ++local_idx) {
        uint32_t local_x = local_idx % WG_SIZE_X;
        uint32_t local_y = local_idx / WG_SIZE_X;

        uint32_t x = gx * WG_SIZE_X + local_x;
        uint32_t y = gy * WG_SIZE_Y + local_y;

        float pixel_luminosity = 0.0f;
        float local_cx = 0.0f;
        float local_cy = 0.0f;

        // Hardened boundary guard
        if (x < cfg.width && y < cfg.height) {
            size_t pixel_idx = (size_t)y * cfg.stride + x;
            size_t word_idx = pixel_idx / 4;
            uint32_t shift_bits = (pixel_idx % 4) * 8;

            uint32_t packed_word = y_plane_buffer[word_idx];
            uint32_t val_u8 = (packed_word >> shift_bits) & 0xFFu;

            pixel_luminosity = (float)val_u8;
            float weight = pixel_luminosity / 255.0f;

            local_cx = (float)x * weight;
            local_cy = (float)y * weight;
        }

        s_lum[local_idx] = pixel_luminosity;
        s_cx[local_idx]  = local_cx;
        s_cy[local_idx]  = local_cy;
    }

    // Phase 2: High-speed parallel tree reduction inside the workgroup
    for (uint32_t stride = 128u; stride > 0u; stride >>= 1u) {
        for (uint32_t local_idx = 0; local_idx < stride; ++local_idx) {
            s_lum[local_idx] += s_lum[local_idx + stride];
            s_cx[local_idx]  += s_cx[local_idx + stride];
            s_cy[local_idx]  += s_cy[local_idx + stride];
        }
    }

    // Phase 3: Quantize float values to fixed-point integers for safe C11 atomic add
    uint32_t local_lum_fixed = (uint32_t)(s_lum[0] * 100.0f);
    uint32_t local_cx_fixed  = (uint32_t)(s_cx[0] * 100.0f);
    uint32_t local_cy_fixed  = (uint32_t)(s_cy[0] * 100.0f);

    atomic_fetch_add_explicit(&global_atomics->global_lum_bits, local_lum_fixed, memory_order_relaxed);
    atomic_fetch_add_explicit(&global_atomics->global_cx_bits, local_cx_fixed, memory_order_relaxed);
    atomic_fetch_add_explicit(&global_atomics->global_cy_bits, local_cy_fixed, memory_order_relaxed);

    uint32_t finished = atomic_fetch_add_explicit(&global_atomics->workgroups_finished, 1u, memory_order_acq_rel);

    // Phase 4: Final block trigger to execute global lighting reductions
    if (finished == total_groups - 1u) {
        float total_samples = (float)(cfg.width * cfg.height);

        if (total_samples > 0.0f) {
            float final_lum = (float)atomic_load_explicit(&global_atomics->global_lum_bits, memory_order_relaxed) / 100.0f;
            float final_cx  = (float)atomic_load_explicit(&global_atomics->global_cx_bits, memory_order_relaxed) / 100.0f;
            float final_cy  = (float)atomic_load_explicit(&global_atomics->global_cy_bits, memory_order_relaxed) / 100.0f;

            float total_weight = final_lum / 255.0f;

            output_data->ambient_intensity = final_lum / (total_samples * 255.0f);
            output_data->color_temp_kelvin = 6500.0f * (output_data->ambient_intensity + 0.5f); // Fixed pointer access ->

            if (total_weight > 0.0f) {
                float mean_cx = final_cx / total_weight;
                float mean_cy = final_cy / total_weight;

                float norm_x = (mean_cx - ((float)cfg.width * 0.5f)) / ((float)cfg.width * 0.5f);
                float norm_y = (mean_cy - ((float)cfg.height * 0.5f)) / ((float)cfg.height * 0.5f);
                float radial_sq = (norm_x * norm_x) + (norm_y * norm_y);
                if (radial_sq > 1.0f) radial_sq = 1.0f;

                struct Vector3 dir = {norm_x, -norm_y, 1.0f - sqrtf(radial_sq)};
                float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                if (len > 0.0001f) {
                    dir.x /= len;
                    dir.y /= len;
                    dir.z /= len;
                }
                output_data->light_dir = dir;
            } else {
                output_data->light_dir = (struct Vector3){0.0f, 1.0f, 0.0f};
            }
        }
    }

    free(arg);
    return NULL;
}

void execute_lighting_pipeline(
    struct PipelineUniforms cfg,
    const uint32_t* y_plane_buffer,
    size_t y_plane_buffer_len,
    struct GlobalAtomicAccumulators* global_atomics,
    struct LightingOutput* output_data
) {
    const uint32_t WG_SIZE_X = 16;
    const uint32_t WG_SIZE_Y = 16;

    uint32_t num_groups_x = (cfg.width + WG_SIZE_X - 1) / WG_SIZE_X;
    uint32_t num_groups_y = (cfg.height + WG_SIZE_Y - 1) / WG_SIZE_Y;
    uint32_t total_groups = num_groups_x * num_groups_y;

    pthread_t* threads = malloc(total_groups * sizeof(pthread_t));
    if (!threads) return;

    size_t thread_count = 0;
    for (uint32_t gy = 0; gy < num_groups_y; ++gy) {
        for (uint32_t gx = 0; gx < num_groups_x; ++gx) {
            struct ThreadArg* arg = malloc(sizeof(struct ThreadArg));
            if (!arg) continue;
            arg->gx = gx;
            arg->gy = gy;
            arg->cfg = cfg;
            arg->y_plane_buffer = y_plane_buffer;
            arg->y_plane_size = y_plane_buffer_len;
            arg->global_atomics = global_atomics;
            arg->output_data = output_data;
            arg->total_groups = total_groups;

            if (pthread_create(&threads[thread_count], NULL, worker_routine, arg) == 0) {
                thread_count++;
            } else {
                free(arg);
            }
        }
    }

    for (size_t i = 0; i < thread_count; ++i) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
}
