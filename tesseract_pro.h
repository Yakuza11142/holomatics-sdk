#ifndef TESSERACT_PRO_H
#define TESSERACT_PRO_H

#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef TESS_EXPORTS
        #define TESS_API __declspec(dllexport)
    #else
        #define TESS_API __declspec(dllimport)
    #endif
#else
    #define TESS_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Forward Context
typedef struct TesseractContext TesseractContext;

// Math Structs
typedef struct { float x, y, z; } TessVec3;
typedef struct { float m[16]; } TessMatrix4x4;
typedef struct { float fx, fy, cx, cy; } TessCameraIntrinsics;

// Frame & Sensor Structs
typedef struct {
    uint8_t* y_plane;
    int32_t width;
    int32_t height;
    int32_t stride;
} TessCameraFrame;

typedef struct {
    float accel[3];
    float gyro[3];
    uint64_t timestamp_ns;
} TessIMUSample;

// Feature Keypoint
typedef struct {
    float x, y;
    float response;
    float angle;
    uint32_t octant;
} TessFeatureKeypoint;

// Core Engine API
TESS_API TesseractContext* tess_create(void);
TESS_API void tess_destroy(TesseractContext* ctx);
TESS_API int32_t tess_process_frame(TesseractContext* ctx, float delta_time);

// Vision & Sensors
TESS_API int32_t tess_cv_extract_fast12_orb(TesseractContext* ctx, const TessCameraFrame* frame);
TESS_API int32_t tess_ekf_propagate_imu(TesseractContext* ctx, const TessIMUSample* sample);
TESS_API void tess_correct_rolling_shutter(TessCameraFrame* frame, const float gyro_velocity[3], float readout_time_seconds);

// RANSAC Solvers
TESS_API int32_t tess_estimate_essential_matrix_ransac(const TessFeatureKeypoint* src, const TessFeatureKeypoint* dst, uint32_t count, uint8_t* inlier_mask);
TESS_API int32_t tess_detect_surface_plane_ransac(const TessVec3* points, uint32_t count, float plane_equation[4]);

// Lighting & Depth Occlusion
TESS_API int32_t tess_estimate_lighting(const TessCameraFrame* frame, float* out_ambient_intensity, float out_light_dir[3], float* out_color_temp_kelvin);
TESS_API int32_t tess_generate_depth_occlusion_mask(const TessCameraIntrinsics* K, const TessVec3* point_cloud, uint32_t point_count, float* depth_buffer, uint32_t width, uint32_t height);

// Map Persistence
TESS_API int32_t tess_map_export_to_file(TesseractContext* ctx, const void* root_node, const char* filepath);
TESS_API int32_t tess_map_import_from_file(TesseractContext* ctx, const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // TESSERACT_PRO_H
