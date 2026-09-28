#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

// ==========================================
// 1. TYPE DEFINITIONS & STRUCTURES
// ==========================================

typedef struct {
    float m[16];
} TessMatrix4x4;

typedef struct TesseractContext TesseractContext;

typedef struct {
    const uint8_t* y_plane;    
    const uint8_t* uv_plane;   
    int32_t width;
    int32_t height;
    int32_t stride;
    float camera_intrinsics[9]; 
} TessCameraFrame;

typedef struct {
    float accel[3];    
    float gyro[3];     
    double timestamp;  
} TessIMUSample;

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

struct WorkgroupResult {
    float lum;
    float cx;
    float cy;
};

struct PoolContext {
    struct PipelineUniforms cfg;
    const uint32_t* y_plane_buffer;
    size_t y_plane_size;
    uint32_t num_groups_x;
    uint32_t num_groups_y;
    uint32_t total_groups;
    uint32_t next_group_idx;
    struct WorkgroupResult* results;
#if defined(_WIN32) || defined(_WIN64)
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
};

struct TesseractContext {
    TessCameraFrame latest_frame;
    bool has_frame;

    TessIMUSample latest_imu;
    bool has_imu;

    struct LightingOutput lighting;
    TessMatrix4x4 current_pose;

#if defined(_WIN32) || defined(_WIN64)
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
};


// ==========================================
// 2. LIGHTING & PIPELINE EXECUTION ENGINE
// ==========================================

static long get_processor_count(void) {
#if defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (long)sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return (count > 0) ? count : 4;
#else
    return 4;
#endif
}

static uint32_t fetch_and_increment_group(struct PoolContext* ctx) {
    uint32_t val;
#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&ctx->mutex);
    val = ctx->next_group_idx;
    ctx->next_group_idx++;
    LeaveCriticalSection(&ctx->mutex);
#else
    pthread_mutex_lock(&ctx->mutex);
    val = ctx->next_group_idx;
    ctx->next_group_idx++;
    pthread_mutex_unlock(&ctx->mutex);
#endif
    return val;
}

static void execute_worker_workload(struct PoolContext* ctx) {
    const uint32_t WG_SIZE_X = 16;
    const uint32_t WG_SIZE_Y = 16;
    const uint32_t LOCAL_SIZE = WG_SIZE_X * WG_SIZE_Y;
    
    uint32_t group_idx;
    uint32_t gx;
    uint32_t gy;
    float s_lum[256];
    float s_cx[256];
    float s_cy[256];
    uint32_t local_idx;
    uint32_t stride;

    for (local_idx = 0; local_idx < 256; ++local_idx) {
        s_lum[local_idx] = 0.0f;
        s_cx[local_idx] = 0.0f;
        s_cy[local_idx] = 0.0f;
    }

    while (true) {
        group_idx = fetch_and_increment_group(ctx);
        if (group_idx >= ctx->total_groups) {
            break;
        }

        gx = group_idx % ctx->num_groups_x;
        gy = group_idx / ctx->num_groups_x;

        for (local_idx = 0; local_idx < LOCAL_SIZE; ++local_idx) {
            uint32_t local_x = local_idx % WG_SIZE_X;
            uint32_t local_y = local_idx / WG_SIZE_X;
            uint32_t x = gx * WG_SIZE_X + local_x;
            uint32_t y = gy * WG_SIZE_Y + local_y;

            float pixel_luminosity = 0.0f;
            float local_cx_val = 0.0f;
            float local_cy_val = 0.0f;

            if (x < ctx->cfg.width && y < ctx->cfg.height && ctx->y_plane_buffer) {
                size_t pixel_idx = (size_t)y * (size_t)ctx->cfg.stride + (size_t)x;
                size_t word_idx = pixel_idx / 4;

                if (word_idx < ctx->y_plane_size) {
                    uint32_t shift_bits = (uint32_t)((pixel_idx % 4) * 8);
                    uint32_t packed_word = ctx->y_plane_buffer[word_idx];
                    uint32_t val_u8 = (packed_word >> shift_bits) & 0xFFu;

                    pixel_luminosity = (float)val_u8;
                    {
                        float weight = pixel_luminosity / 255.0f;
                        local_cx_val = (float)x * weight;
                        local_cy_val = (float)y * weight;
                    }
                }
            }

            s_lum[local_idx] = pixel_luminosity;
            s_cx[local_idx]  = local_cx_val;
            s_cy[local_idx]  = local_cy_val;
        }

        for (stride = 128u; stride > 0u; stride >>= 1u) {
            for (local_idx = 0; local_idx < stride; ++local_idx) {
                s_lum[local_idx] += s_lum[local_idx + stride];
                s_cx[local_idx]  += s_cx[local_idx + stride];
                s_cy[local_idx]  += s_cy[local_idx + stride];
            }
        }

        ctx->results[group_idx].lum = s_lum[0];
        ctx->results[group_idx].cx  = s_cx[0];
        ctx->results[group_idx].cy  = s_cy[0];
    }
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI win_worker_routine(LPVOID lpParam) {
    execute_worker_workload((struct PoolContext*)lpParam);
    return 0;
}
#else
static void* posix_worker_routine(void* arg) {
    execute_worker_workload((struct PoolContext*)arg);
    return NULL;
}
#endif

static bool execute_lighting_pipeline(
    struct PipelineUniforms cfg,
    const uint32_t* y_plane_buffer,
    size_t y_plane_buffer_len,
    void* unused_atomics,
    struct LightingOutput* output_data
) {
    const uint32_t WG_SIZE_X = 16;
    const uint32_t WG_SIZE_Y = 16;
    uint32_t num_groups_x;
    uint32_t num_groups_y;
    uint32_t total_groups;
    struct WorkgroupResult* results;
    long processors;
    size_t thread_count;
    struct PoolContext ctx;
#if defined(_WIN32) || defined(_WIN64)
    HANDLE* threads;
#else
    pthread_t* threads;
#endif
    size_t spawned_threads;
    size_t i;
    double total_lum;
    double total_cx;
    double total_cy;
    float total_samples;
    float final_lum;
    float final_cx;
    float final_cy;
    float total_weight;
    float mean_cx;
    float mean_cy;
    float norm_x;
    float norm_y;
    float radial_sq;
    struct Vector3 dir;
    float len;

    (void)unused_atomics;

    if (cfg.width == 0 || cfg.height == 0 || cfg.stride < cfg.width || !y_plane_buffer || !output_data) {
        return false;
    }

    num_groups_x = (cfg.width + WG_SIZE_X - 1) / WG_SIZE_X;
    num_groups_y = (cfg.height + WG_SIZE_Y - 1) / WG_SIZE_Y;
    total_groups = num_groups_x * num_groups_y;

    if (total_groups == 0) return false;

    results = (struct WorkgroupResult*)malloc(total_groups * sizeof(struct WorkgroupResult));
    if (!results) {
        return false;
    }

    processors = get_processor_count();
    if (processors < 1) processors = 4;
    thread_count = (size_t)processors;
    if (thread_count > (size_t)total_groups) {
        thread_count = (size_t)total_groups;
    }

    ctx.cfg = cfg;
    ctx.y_plane_buffer = y_plane_buffer;
    ctx.y_plane_size = y_plane_buffer_len;
    ctx.num_groups_x = num_groups_x;
    ctx.num_groups_y = num_groups_y;
    ctx.total_groups = total_groups;
    ctx.next_group_idx = 0;
    ctx.results = results;

#if defined(_WIN32) || defined(_WIN64)
    InitializeCriticalSection(&ctx.mutex);
    threads = (HANDLE*)malloc(thread_count * sizeof(HANDLE));
#else
    pthread_mutex_init(&ctx.mutex, NULL);
    threads = (pthread_t*)malloc(thread_count * sizeof(pthread_t));
#endif

    if (!threads) {
        free(results);
#if defined(_WIN32) || defined(_WIN64)
        DeleteCriticalSection(&ctx.mutex);
#else
        pthread_mutex_destroy(&ctx.mutex);
#endif
        return false;
    }

    spawned_threads = 0;
    for (i = 0; i < thread_count; ++i) {
#if defined(_WIN32) || defined(_WIN64)
        threads[i] = CreateThread(NULL, 0, win_worker_routine, &ctx, 0, NULL);
        if (threads[i] != NULL) {
            spawned_threads++;
        } else {
            break;
        }
#else
        if (pthread_create(&threads[i], NULL, posix_worker_routine, &ctx) == 0) {
            spawned_threads++;
        } else {
            break;
        }
#endif
    }

    if (spawned_threads == 0) {
        free(threads);
        free(results);
#if defined(_WIN32) || defined(_WIN64)
        DeleteCriticalSection(&ctx.mutex);
#else
        pthread_mutex_destroy(&ctx.mutex);
#endif
        return false;
    }

#if defined(_WIN32) || defined(_WIN64)
    WaitForMultipleObjects((DWORD)spawned_threads, threads, TRUE, INFINITE);
    for (i = 0; i < spawned_threads; ++i) {
        CloseHandle(threads[i]);
    }
    DeleteCriticalSection(&ctx.mutex);
#else
    for (i = 0; i < spawned_threads; ++i) {
        pthread_join(threads[i], NULL);
    }
    pthread_mutex_destroy(&ctx.mutex);
#endif

    free(threads);

    total_lum = 0.0;
    total_cx = 0.0;
    total_cy = 0.0;

    for (i = 0; i < (size_t)total_groups; ++i) {
        total_lum += (double)results[i].lum;
        total_cx  += (double)results[i].cx;
        total_cy  += (double)results[i].cy;
    }

    free(results);

    total_samples = (float)((uint64_t)cfg.width * (uint64_t)cfg.height);
    if (total_samples > 0.0f) {
        final_lum = (float)total_lum;
        final_cx  = (float)total_cx;
        final_cy  = (float)total_cy;

        total_weight = final_lum / 255.0f;

        output_data->ambient_intensity = final_lum / (total_samples * 255.0f);
        output_data->color_temp_kelvin = 6500.0f * (output_data->ambient_intensity + 0.5f);

        if (total_weight > 0.0f) {
            mean_cx = final_cx / total_weight;
            mean_cy = final_cy / total_weight;

            norm_x = (mean_cx - ((float)cfg.width * 0.5f)) / ((float)cfg.width * 0.5f);
            norm_y = (mean_cy - ((float)cfg.height * 0.5f)) / ((float)cfg.height * 0.5f);
            radial_sq = (norm_x * norm_x) + (norm_y * norm_y);
            if (radial_sq > 1.0f) radial_sq = 1.0f;

            dir.x = norm_x;
            dir.y = -norm_y;
            dir.z = 1.0f - sqrtf(radial_sq);
            
            len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
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

    return true;
}


// ==========================================
// 3. PUBLIC SENSOR API IMPLEMENTATION
// ==========================================

TesseractContext* tesseract_context_create(void) {
    TesseractContext* ctx;
    int i;

    ctx = (TesseractContext*)malloc(sizeof(TesseractContext));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(TesseractContext));
    
    for (i = 0; i < 16; ++i) {
        ctx->current_pose.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }

#if defined(_WIN32) || defined(_WIN64)
    InitializeCriticalSection(&ctx->mutex);
#else
    pthread_mutex_init(&ctx->mutex, NULL);
#endif

    return ctx;
}

void tesseract_context_destroy(TesseractContext* ctx) {
    if (!ctx) return;

#if defined(_WIN32) || defined(_WIN64)
    DeleteCriticalSection(&ctx->mutex);
#else
    pthread_mutex_destroy(&ctx->mutex);
#endif

    free(ctx);
}

int32_t tess_push_camera_frame(TesseractContext* ctx, const TessCameraFrame* frame) {
    struct PipelineUniforms cfg;
    const uint32_t* y_word_buffer;
    size_t y_buffer_len;
    struct LightingOutput computed_lighting;
    bool success;

    if (!ctx || !frame || frame->width <= 0 || frame->height <= 0 || !frame->y_plane) {
        return -1;
    }

#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&ctx->mutex);
#else
    pthread_mutex_lock(&ctx->mutex);
#endif

    ctx->latest_frame = *frame;
    ctx->has_frame = true;

    cfg.width = (uint32_t)frame->width;
    cfg.height = (uint32_t)frame->height;
    cfg.stride = (uint32_t)(frame->stride > 0 ? frame->stride : frame->width);
    cfg.total_workgroups = 0;

    y_word_buffer = (const uint32_t*)frame->y_plane;
    y_buffer_len = ((size_t)cfg.stride * (size_t)cfg.height + 3) / 4;

    success = execute_lighting_pipeline(cfg, y_word_buffer, y_buffer_len, NULL, &computed_lighting);
    if (success) {
        ctx->lighting = computed_lighting;
    }

#if defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&ctx->mutex);
#else
    pthread_mutex_unlock(&ctx->mutex);
#endif

    return success ? 0 : -2;
}

int32_t tess_push_imu_sample(TesseractContext* ctx, const TessIMUSample* sample) {
    float ax;
    float ay;
    float az;

    if (!ctx || !sample) {
        return -1;
    }

#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&ctx->mutex);
#else
    pthread_mutex_lock(&ctx->mutex);
#endif

    ctx->latest_imu = *sample;
    ctx->has_imu = true;

    ax = sample->accel[0];
    ay = sample->accel[1];
    az = sample->accel[2];

    ctx->current_pose.m[12] += ax * 0.001f;
    ctx->current_pose.m[13] += ay * 0.001f;
    ctx->current_pose.m[14] += az * 0.001f;

#if defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&ctx->mutex);
#else
    pthread_mutex_unlock(&ctx->mutex);
#endif

    return 0;
}

int32_t tess_get_pose_estimate(TesseractContext* ctx, TessMatrix4x4* out_pose) {
    bool has_data;

    if (!ctx || !out_pose) {
        return -1;
    }

#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&ctx->mutex);
#else
    pthread_mutex_lock(&ctx->mutex);
#endif

    *out_pose = ctx->current_pose;
    has_data = ctx->has_frame || ctx->has_imu;

#if defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&ctx->mutex);
#else
    pthread_mutex_unlock(&ctx->mutex);
#endif

    return has_data ? 0 : 1;
}
