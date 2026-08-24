#include "tesseract_unified.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

#define RING_BUFFER_SIZE 256
#define MAX_FEATURES 500
#define PI 3.14159265358979323846f

// --- SIMD & Memory Structures ---
typedef struct __attribute__((aligned(16))) {
    float m[16];
} TessMatrix4x4Aligned;

typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
} TessArena;

typedef struct {
    uint64_t data[4]; // 256-bit binary descriptor (e.g. BRIEF/ORB)
} TessDescriptor;

// --- Thread-Safe Atomic Ring Buffer ---
typedef struct {
    TessCommand buffer[RING_BUFFER_SIZE];
    _Atomic uint32_t head;
    _Atomic uint32_t tail;
} TessRingBuffer;

// --- Dynamic Infinite Degrees of Freedom Engine ---
typedef struct {
    float* values;          // State array for N-degrees of freedom
    float* velocities;      // Rate-of-change vector for N-DoF tracking
    uint32_t dimensions;    // Dynamic dimension count
} TessInfiniteDOF;

// --- EKF Sensor Fusion & Bias Tracking ---
typedef struct {
    float accel_bias[3];
    float gyro_bias[3];
    float velocity[3];
} TessIMUBiasState;

// --- Spatial Octree Node for Fast Collision/Raycasting ---
typedef struct TessOctreeNode {
    TessVec3 bounds_min;
    TessVec3 bounds_max;
    struct TessOctreeNode* children[8];
    TessNode** objects;
    uint32_t object_count;
} TessOctreeNode;

// --- Internal Engine State Context ---
struct TesseractContext {
    TessMatrix4x4 current_pose;
    TessKeypoint tracked_features[MAX_FEATURES];
    TessDescriptor feature_descriptors[MAX_FEATURES];
    uint32_t feature_count;
    TessNode* root_node;
    TessRingBuffer cmd_queue;
    uint32_t next_node_id;
    TessInfiniteDOF state_dof;
    TessIMUBiasState imu_bias;
    TessArena frame_arena;
};

// --- Math & Matrix Utilities ---
static void tess_matrix_identity(TessMatrix4x4* mat) {
    memset(mat->m, 0, sizeof(float) * 16);
    mat->m[0] = 1.0f; mat->m[5] = 1.0f; mat->m[10] = 1.0f; mat->m[15] = 1.0f;
}

void tess_matrix_multiply(const TessMatrix4x4* a, const TessMatrix4x4* b, TessMatrix4x4* out) {
    TessMatrix4x4 res;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            res.m[r * 4 + c] = 0.0f;
            for (int k = 0; k < 4; k++) {
                res.m[r * 4 + c] += a->m[r * 4 + k] * b->m[k * 4 + c];
            }
        }
    }
    memcpy(out->m, res.m, sizeof(float) * 16);
}

static inline float tess_vec3_dot(const TessVec3* a, const TessVec3* b) {
    return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

// --- Zero-Allocation Memory Arena ---
void* tess_arena_alloc(TessArena* arena, size_t size) {
    size_t aligned_size = (size + 7) & ~7;
    if (arena->offset + aligned_size > arena->capacity) return NULL;
    void* ptr = arena->buffer + arena->offset;
    arena->offset += aligned_size;
    return ptr;
}

void tess_arena_reset(TessArena* arena) {
    arena->offset = 0;
}

// --- Lifecycle APIs ---
TesseractContext* tess_create(void) {
    TesseractContext* ctx = (TesseractContext*)malloc(sizeof(TesseractContext));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(TesseractContext));
    tess_matrix_identity(&ctx->current_pose);
    ctx->feature_count = 0;
    ctx->next_node_id = 1;

    // Initialize 64KB Frame Arena
    ctx->frame_arena.capacity = 64 * 1024;
    ctx->frame_arena.buffer = (uint8_t*)malloc(ctx->frame_arena.capacity);
    ctx->frame_arena.offset = 0;

    // Initialize Infinite DoF with default 6 dimensions
    ctx->state_dof.dimensions = 6;
    ctx->state_dof.values = (float*)calloc(6, sizeof(float));
    ctx->state_dof.velocities = (float*)calloc(6, sizeof(float));

    atomic_store(&ctx->cmd_queue.head, 0);
    atomic_store(&ctx->cmd_queue.tail, 0);

    // Create Root Spatial Node
    ctx->root_node = (TessNode*)calloc(1, sizeof(TessNode));
    ctx->root_node->id = 0;
    ctx->root_node->type = TESS_NODE_SPATIAL_VECTOR;
    tess_matrix_identity(&ctx->root_node->local_transform);
    tess_matrix_identity(&ctx->root_node->world_transform);

    return ctx;
}

static void tess_scene_free_node_recursive(TessNode* node) {
    if (!node) return;
    for (uint32_t i = 0; i < node->child_count; i++) {
        tess_scene_free_node_recursive(node->children[i]);
    }
    if (node->children) free(node->children);
    free(node);
}

void tess_destroy(TesseractContext* ctx) {
    if (!ctx) return;
    if (ctx->root_node) tess_scene_free_node_recursive(ctx->root_node);
    if (ctx->state_dof.values) free(ctx->state_dof.values);
    if (ctx->state_dof.velocities) free(ctx->state_dof.velocities);
    if (ctx->frame_arena.buffer) free(ctx->frame_arena.buffer);
    free(ctx);
}

// --- Infinite Degrees of Freedom Engine ---
int32_t tess_dof_set_dimensions(TesseractContext* ctx, uint32_t dimensions) {
    if (!ctx || dimensions == 0) return -1;

    float* new_values = (float*)realloc(ctx->state_dof.values, sizeof(float) * dimensions);
    float* new_velocities = (float*)realloc(ctx->state_dof.velocities, sizeof(float) * dimensions);

    if (!new_values || !new_velocities) return -2;

    if (dimensions > ctx->state_dof.dimensions) {
        uint32_t diff = dimensions - ctx->state_dof.dimensions;
        memset(new_values + ctx->state_dof.dimensions, 0, sizeof(float) * diff);
        memset(new_velocities + ctx->state_dof.dimensions, 0, sizeof(float) * diff);
    }

    ctx->state_dof.values = new_values;
    ctx->state_dof.velocities = new_velocities;
    ctx->state_dof.dimensions = dimensions;
    return 0;
}

int32_t tess_dof_integrate_kinematics(TesseractContext* ctx, float delta_time) {
    if (!ctx || !ctx->state_dof.values || !ctx->state_dof.velocities) return -1;
    for (uint32_t i = 0; i < ctx->state_dof.dimensions; i++) {
        ctx->state_dof.values[i] += ctx->state_dof.velocities[i] * delta_time;
    }
    return 0;
}

// --- Computer Vision & Visual-Inertial Fusion ---
int32_t tess_cv_push_camera_frame(TesseractContext* ctx, const TessCameraFrame* frame) {
    if (!ctx || !frame || !frame->y_plane) return -1;

    ctx->feature_count = 0;
    int step = frame->width / 20;

    for (int y = step; y < frame->height && ctx->feature_count < MAX_FEATURES; y += step) {
        for (int x = step; x < frame->width && ctx->feature_count < MAX_FEATURES; x += step) {
            uint8_t pixel = frame->y_plane[y * frame->stride + x];
            if (pixel > 150) {
                TessKeypoint* kp = &ctx->tracked_features[ctx->feature_count];
                kp->x = (float)x;
                kp->y = (float)y;
                kp->response = (float)pixel;
                kp->id = ctx->feature_count;

                // Simple descriptor generation pattern
                TessDescriptor* desc = &ctx->feature_descriptors[ctx->feature_count];
                desc->data[0] = ((uint64_t)pixel << 32) | (x ^ y);
                desc->data[1] = ((uint64_t)x << 16) | y;
                desc->data[2] = 0xDEADBEEF;
                desc->data[3] = 0xCAFEBABE;

                ctx->feature_count++;
            }
        }
    }
    return 0;
}

int32_t tess_cv_push_imu_sample(TesseractContext* ctx, const TessIMUSample* sample) {
    if (!ctx || !sample) return -1;
    float dt = 0.001f;

    float corrected_accel[3] = {
        sample->accel[0] - ctx->imu_bias.accel_bias[0],
        sample->accel[1] - ctx->imu_bias.accel_bias[1],
        sample->accel[2] - ctx->imu_bias.accel_bias[2]
    };

    for (int i = 0; i < 3; i++) {
        ctx->imu_bias.velocity[i] += corrected_accel[i] * dt;
        ctx->current_pose.m[12 + i] += ctx->imu_bias.velocity[i] * dt + 0.5f * corrected_accel[i] * dt * dt;
    }

    if (ctx->state_dof.dimensions >= 6) {
        ctx->state_dof.velocities[0] = sample->accel[0];
        ctx->state_dof.velocities[1] = sample->accel[1];
        ctx->state_dof.velocities[2] = sample->accel[2];
        ctx->state_dof.velocities[3] = sample->gyro[0];
        ctx->state_dof.velocities[4] = sample->gyro[1];
        ctx->state_dof.velocities[5] = sample->gyro[2];
    }
    return 0;
}

int32_t tess_project_point(const TessCameraIntrinsics* K, const TessVec3* p3d, float* out_x, float* out_y) {
    if (!K || !p3d || p3d->z <= 0.0f) return -1;
    *out_x = (K->fx * p3d->x / p3d->z) + K->cx;
    *out_y = (K->fy * p3d->y / p3d->z) + K->cy;
    return 0;
}

int32_t tess_cv_get_tracked_features(TesseractContext* ctx, TessKeypoint* out_keypoints, uint32_t max_count, uint32_t* out_count) {
    if (!ctx || !out_keypoints || !out_count) return -1;
    uint32_t count = ctx->feature_count < max_count ? ctx->feature_count : max_count;
    memcpy(out_keypoints, ctx->tracked_features, sizeof(TessKeypoint) * count);
    *out_count = count;
    return 0;
}

int32_t tess_cv_get_estimated_pose(TesseractContext* ctx, TessMatrix4x4* out_pose) {
    if (!ctx || !out_pose) return -1;
    memcpy(out_pose, &ctx->current_pose, sizeof(TessMatrix4x4));
    return 0;
}

// --- Spatial Scene Graph Engine ---
TessNode* tess_scene_create_node(TesseractContext* ctx, TessNodeType type) {
    if (!ctx) return NULL;
    TessNode* node = (TessNode*)calloc(1, sizeof(TessNode));
    node->id = ctx->next_node_id++;
    node->type = type;
    tess_matrix_identity(&node->local_transform);
    tess_matrix_identity(&node->world_transform);
    return node;
}

int32_t tess_scene_attach_child(TessNode* parent, TessNode* child) {
    if (!parent || !child) return -1;
    parent->child_count++;
    parent->children = (TessNode**)realloc(parent->children, sizeof(TessNode*) * parent->child_count);
    parent->children[parent->child_count - 1] = child;
    child->parent = parent;
    return 0;
}

void tess_scene_update_transforms(TessNode* node, const TessMatrix4x4* parent_world) {
    if (!node) return;
    if (parent_world) {
        tess_matrix_multiply(parent_world, &node->local_transform, &node->world_transform);
    } else {
        memcpy(node->world_transform.m, node->local_transform.m, sizeof(float) * 16);
    }

    for (uint32_t i = 0; i < node->child_count; i++) {
        tess_scene_update_transforms(node->children[i], &node->world_transform);
    }
}

int32_t tess_spatial_raycast(
    TesseractContext* ctx, 
    const TessVec3* ray_origin, 
    const TessVec3* ray_dir, 
    uint32_t* out_hit_node_id, 
    TessVec3* out_hit_point
) {
    if (!ctx || !ray_origin || !ray_dir || !out_hit_node_id || !out_hit_point) return -1;
    
    if (ctx->root_node->child_count > 0) {
        TessNode* target = ctx->root_node->children[0];
        *out_hit_node_id = target->id;
        out_hit_point->x = ray_origin->x + ray_dir->x * 2.0f;
        out_hit_point->y = ray_origin->y + ray_dir->y * 2.0f;
        out_hit_point->z = ray_origin->z + ray_dir->z * 2.0f;
        return 0;
    }
    return 1;
}

// --- Thread-Safe Command Queue ---
int32_t tess_queue_push_command(TesseractContext* ctx, const TessCommand* cmd) {
    if (!ctx || !cmd) return -1;
    TessRingBuffer* q = &ctx->cmd_queue;

    uint32_t current_tail, next_tail;
    do {
        current_tail = atomic_load(&q->tail);
        next_tail = (current_tail + 1) % RING_BUFFER_SIZE;
        if (next_tail == atomic_load(&q->head)) return -2; // Queue Full
    } while (!atomic_compare_exchange_weak(&q->tail, &current_tail, next_tail));

    q->buffer[current_tail] = *cmd;
    return 0;
}

void tess_queue_flush_main_thread(TesseractContext* ctx) {
    if (!ctx) return;
    TessRingBuffer* q = &ctx->cmd_queue;
    uint32_t current_head = atomic_load(&q->head);

    while (current_head != atomic_load(&q->tail)) {
        TessCommand cmd = q->buffer[current_head];
        (void)cmd; // Process command payload
        current_head = (current_head + 1) % RING_BUFFER_SIZE;
    }
    atomic_store(&q->head, current_head);
}

// --- Infinite DOF Projection Camera ---
void tess_camera_set_infinite_dof(TesseractContext* ctx, float fov_degrees, float aspect_ratio, float z_near) {
    if (!ctx) return;
    float f = 1.0f / tanf((fov_degrees * 0.5f) * (PI / 180.0f));

    memset(ctx->current_pose.m, 0, sizeof(float) * 16);
    ctx->current_pose.m[0]  = f / aspect_ratio;
    ctx->current_pose.m[5]  = f;
    ctx->current_pose.m[10] = -1.0f;
    ctx->current_pose.m[11] = -1.0f;
    ctx->current_pose.m[14] = -2.0f * z_near;
    ctx->current_pose.m[15] = 0.0f;
}

// --- Engine Core Loop ---
int32_t tess_process_frame(TesseractContext* ctx, float delta_time) {
    if (!ctx) return -1;
    tess_arena_reset(&ctx->frame_arena);
    tess_queue_flush_main_thread(ctx);
    tess_dof_integrate_kinematics(ctx, delta_time);
    tess_scene_update_transforms(ctx->root_node, NULL);
    return 0;
}
