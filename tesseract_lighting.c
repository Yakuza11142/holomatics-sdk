#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <cmath>
#include <span>
#include <algorithm>

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

struct LightingOutput {
    float ambient_intensity;
    float color_temp_kelvin;
    struct Vector3 {
        float x, y, z;
    } light_dir;
};

struct GlobalAtomicAccumulators {
    std::atomic<float> global_lum_bits{0.0f};
    std::atomic<float> global_cx_bits{0.0f};
    std::atomic<float> global_cy_bits{0.0f};
    std::atomic<uint32_t> workgroups_finished{0};
};

void execute_lighting_pipeline(
    const PipelineUniforms& cfg,
    std::span<const uint32_t> y_plane_buffer,
    GlobalAtomicAccumulators& global_atomics,
    LightingOutput& output_data
) {
    constexpr uint32_t WG_SIZE_X = 16;
    constexpr uint32_t WG_SIZE_Y = 16;
    constexpr uint32_t LOCAL_SIZE = WG_SIZE_X * WG_SIZE_Y; // 256

    uint32_t num_groups_x = (cfg.width + WG_SIZE_X - 1) / WG_SIZE_X;
    uint32_t num_groups_y = (cfg.height + WG_SIZE_Y - 1) / WG_SIZE_Y;
    uint32_t total_groups = num_groups_x * num_groups_y;

    // C++20 jthreads automatically handle safe RAII thread joining on scope exit
    std::vector<std::jthread> workers;
    workers.reserve(total_groups);

    for (uint32_t gy = 0; gy < num_groups_y; ++gy) {
        for (uint32_t gx = 0; gx < num_groups_x; ++gx) {
            workers.emplace_back([=, &y_plane_buffer, &global_atomics, &output_data]() {
                // Emulated shared workgroup memory arrays
                float s_lum[LOCAL_SIZE] = {0.0f};
                float s_cx[LOCAL_SIZE] = {0.0f};
                float s_cy[LOCAL_SIZE] = {0.0f};

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
                        size_t pixel_idx = static_cast<size_t>(y) * cfg.stride + x;
                        size_t word_idx = pixel_idx / 4;
                        uint32_t shift_bits = (pixel_idx % 4) * 8;

                        if (word_idx < y_plane_buffer.size()) {
                            uint32_t packed_word = y_plane_buffer[word_idx];
                            uint32_t val_u8 = (packed_word >> shift_bits) & 0xFFu;

                            pixel_luminosity = static_cast<float>(val_u8);
                            float weight = pixel_luminosity / 255.0f;

                            local_cx = static_cast<float>(x) * weight;
                            local_cy = static_cast<float>(y) * weight;
                        }
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

                // Phase 3: Native C++20 floating-point atomic accumulation globally
                global_atomics.global_lum_bits.fetch_add(s_lum[0], std::memory_order_relaxed);
                global_atomics.global_cx_bits.fetch_add(s_cx[0], std::memory_order_relaxed);
                global_atomics.global_cy_bits.fetch_add(s_cy[0], std::memory_order_relaxed);

                uint32_t finished = global_atomics.workgroups_finished.fetch_add(1u, std::memory_order_acq_rel);

                // Phase 4: Final block trigger to execute global lighting reductions
                if (finished == total_groups - 1u) {
                    float total_samples = static_cast<float>(cfg.width * cfg.height);

                    if (total_samples > 0.0f) {
                        float final_lum = global_atomics.global_lum_bits.load(std::memory_order_relaxed);
                        float final_cx  = global_atomics.global_cx_bits.load(std::memory_order_relaxed);
                        float final_cy  = global_atomics.global_cy_bits.load(std::memory_order_relaxed);

                        float total_weight = final_lum / 255.0f;

                        output_data.ambient_intensity = final_lum / (total_samples * 255.0f);
                        output_data.color_temp_kelvin = 6500.0f * (output_data.ambient_intensity + 0.5f);

                        if (total_weight > 0.0f) {
                            float mean_cx = final_cx / total_weight;
                            float mean_cy = final_cy / total_weight;

                            float norm_x = (mean_cx - (static_cast<float>(cfg.width) * 0.5f)) / (static_cast<float>(cfg.width) * 0.5f);
                            float norm_y = (mean_cy - (static_cast<float>(cfg.height) * 0.5f)) / (static_cast<float>(cfg.height) * 0.5f);
                            float radial_sq = std::min(1.0f, (norm_x * norm_x) + (norm_y * norm_y));

                            LightingOutput::Vector3 dir = {norm_x, -norm_y, 1.0f - std::sqrt(radial_sq)};
                            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                            if (len > 0.0001f) {
                                dir.x /= len;
                                dir.y /= len;
                                dir.z /= len;
                            }
                            output_data.light_dir = dir;
                        } else {
                            output_data.light_dir = {0.0f, 1.0f, 0.0f};
                        }
                    }
                }
            });
        }
    }
}
