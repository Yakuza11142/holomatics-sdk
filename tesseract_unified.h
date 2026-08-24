#ifndef TESSERACT_UNIFIED_H
#define TESSERACT_UNIFIED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 1. DATA STRUCTURES & TYPES
// ============================================================================

typedef struct TesseractContext TesseractContext;

// 4x4 Matrix (Column-Major for OpenGL/Vulkan/Metal compatibility)
typedef struct {
    float m[16];
} TessMatrix4x4;

// 3D Vector
typedef struct {
    float x, y, z;
} TessVec3;

// 2D Feature Point for Computer Vision / Tracking
typedef struct {
    float x, y;
    float response;
    uint32_t id;
} TessKeypoint;

// Hardware Camera Frame (YUV / Grayscale Zero-Copy Input)
typedef struct {
    const uint8_t* y_plane;    // Luminance plane (Used for CV feature tracking)
    const uint8_t* uv_plane;   // Chrominance plane
    int32_t width;
    int32_t height;
    int32_t stride;
    float intrinsics[9];       // Camera Matrix: [fx 0 cx, 0 fy cy, 0 0 1]
    double timestamp;
} TessCameraFrame;

// Hardware IMU Sample
typedef struct {
    float accel[3];            // Accelerometer: X, Y, Z (m/s^2)
    float gyro[3];             // Gyroscope: Pitch, Roll, Yaw (rad/s)
    double timestamp;
} TessIMUSample;

// Spatial Scene Graph Node Types
typedef enum {
    TESS_NODE_SPATIAL_VECTOR = 0,
    TESS_NODE_3D_MESH        = 1,
    TESS_NODE_UI_CANVAS      = 2
} TessNodeType;

// Spatial Scene Node Structure
typedef struct TessNode {
    uint32_t id;
    TessNodeType type;
    TessMatrix4x4 local_transform;
    TessMatrix4x4 world_transform;
    TessVec3 bbox_min;
    TessVec3 bbox_max;
    struct TessNode* parent;
    struct TessNode** children;
    uint32_t child_count;
} TessNode;

// ============================================================================
// 2. LIFECYCLE & CORE ENGINE APIs
// ============================================================================

TesseractContext* tess_create(void);
void tess_destroy(TesseractContext* ctx);
int32_t tess_process_frame(TesseractContext* ctx, float delta_time);

// ============================================================================
// 3. COMPUTER VISION & HARDWARE SLAM ENGINE
// ============================================================================

// Push raw hardware inputs into engine buffers
int32_t tess_cv_push_camera_frame(TesseractContext* ctx, const TessCameraFrame* frame);
int32_t tess_cv_push_imu_sample(TesseractContext* ctx, const TessIMUSample* sample);

// Extract tracked visual feature points (FAST / KLT Feature Engine)
int32_t tess_cv_get_tracked_features(TesseractContext* ctx, TessKeypoint* out_keypoints, uint32_t max_count, uint32_t* out_count);

// Retrieve predicted 6-DoF Camera Pose Matrix from fused Visual-Inertial Odometry
int32_t tess_cv_get_estimated_pose(TesseractContext* ctx, TessMatrix4x4* out_pose);

// ============================================================================
// 4. SPATIAL COMPUTING & SCENE GRAPH ENGINE
// ============================================================================

TessNode* tess_scene_create_node(TesseractContext* ctx, TessNodeType type);
int32_t tess_scene_attach_child(TessNode* parent, TessNode* child);
int32_t tess_transform_vector(TesseractContext* ctx, const float* in_vec3, float* out_vec3);

// Spatial Raycasting for 3D Hit Testing & UI Interaction
int32_t tess_spatial_raycast(
    TesseractContext* ctx, 
    const TessVec3* ray_origin, 
    const TessVec3* ray_direction, 
    uint32_t* out_hit_node_id, 
    TessVec3* out_hit_point
);

// ============================================================================
// 5. THREAD-SAFE COMMAND QUEUE ENGINE (LOCK-FREE API)
// ============================================================================

typedef struct {
    uint32_t command_id;
    float payload_data[16];
} TessCommand;

int32_t tess_queue_push_command(TesseractContext* ctx, const TessCommand* cmd);
void tess_queue_flush_main_thread(TesseractContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // TESSERACT_UNIFIED_H
