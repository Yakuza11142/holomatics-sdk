#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <pthread.h>
#endif

// ==========================================
// 1. TYPE DEFINITIONS & STRUCTURES
// ==========================================

typedef struct {
    float m[16];
} TessMatrix4x4;

typedef struct TesseractContext TesseractContext;

typedef enum {
    TESS_NODE_SPATIAL_VECTOR = 0,
    TESS_NODE_3D_MESH        = 1,
    TESS_NODE_UI_CANVAS      = 2
} TessNodeType;

typedef struct TessNode {
    uint32_t id;
    TessNodeType type;
    TessMatrix4x4 local_transform;
    TessMatrix4x4 world_transform;

    // Bounding Box for Hit-Testing & Raycasting
    float bbox_min[3];
    float bbox_max[3];

    struct TessNode* parent;
    struct TessNode** children;
    uint32_t child_count;
    uint32_t child_capacity;
} TessNode;

// Complete Tesseract Context Definition to support scene graph roots & tracking
struct TesseractContext {
    TessNode** root_nodes;
    uint32_t root_count;
    uint32_t root_capacity;
    uint32_t next_node_id;

#if defined(_WIN32) || defined(_WIN64)
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
};


// ==========================================
// 2. MATRIX MATH & TRANSFORM UTILITIES
// ==========================================

static void tess_matrix_identity(TessMatrix4x4* out) {
    int i;
    for (i = 0; i < 16; ++i) {
        out->m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

static void tess_matrix_multiply(const TessMatrix4x4* a, const TessMatrix4x4* b, TessMatrix4x4* out) {
    TessMatrix4x4 temp;
    int row, col, i;

    for (row = 0; row < 4; ++row) {
        for (col = 0; col < 4; ++col) {
            float sum = 0.0f;
            for (i = 0; i < 4; ++i) {
                sum += a->m[i * 4 + row] * b->m[col * 4 + i];
            }
            temp.m[col * 4 + row] = sum;
        }
    }
    *out = temp;
}

static void update_world_transforms_recursive(TessNode* node, const TessMatrix4x4* parent_transform) {
    uint32_t i;

    if (parent_transform) {
        tess_matrix_multiply(parent_transform, &node->local_transform, &node->world_transform);
    } else {
        node->world_transform = node->local_transform;
    }

    for (i = 0; i < node->child_count; ++i) {
        if (node->children[i]) {
            update_world_transforms_recursive(node->children[i], &node->world_transform);
        }
    }
}


// ==========================================
// 3. SCENE GRAPH API IMPLEMENTATION
// ==========================================

// Helper to create context if initialized through scene hooks
TesseractContext* tesseract_scene_context_create(void) {
    TesseractContext* ctx = (TesseractContext*)malloc(sizeof(TesseractContext));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(TesseractContext));
    ctx->next_node_id = 1;

#if defined(_WIN32) || defined(_WIN64)
    InitializeCriticalSection(&ctx->mutex);
#else
    pthread_mutex_init(&ctx->mutex, NULL);
#endif

    return ctx;
}

void tesseract_scene_context_destroy(TesseractContext* ctx) {
    uint32_t i;
    if (!ctx) return;

#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&ctx->mutex);
#else
    pthread_mutex_lock(&ctx->mutex);
#endif

    // Free root nodes recursively (omitting full deep destructor for brevity, can be expanded)
    for (i = 0; i < ctx->root_count; ++i) {
        if (ctx->root_nodes[i]) {
            if (ctx->root_nodes[i]->children) free(ctx->root_nodes[i]->children);
            free(ctx->root_nodes[i]);
        }
    }
    if (ctx->root_nodes) free(ctx->root_nodes);

#if defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&ctx->mutex);
    DeleteCriticalSection(&ctx->mutex);
#else
    pthread_mutex_unlock(&ctx->mutex);
    pthread_mutex_destroy(&ctx->mutex);
#endif

    free(ctx);
}

TessNode* tess_scene_create_node(TesseractContext* ctx, TessNodeType type) {
    TessNode* node;

    if (!ctx) return NULL;

    node = (TessNode*)malloc(sizeof(TessNode));
    if (!node) return NULL;

    memset(node, 0, sizeof(TessNode));

#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&ctx->mutex);
#else
    pthread_mutex_lock(&ctx->mutex);
#endif

    node->id = ctx->next_node_id++;

#if defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&ctx->mutex);
#else
    pthread_mutex_unlock(&ctx->mutex);
#endif

    node->type = type;
    tess_matrix_identity(&node->local_transform);
    tess_matrix_identity(&node->world_transform);

    // Default unit bounding box [-0.5, 0.5]
    node->bbox_min[0] = -0.5f; node->bbox_min[1] = -0.5f; node->bbox_min[2] = -0.5f;
    node->bbox_max[0] =  0.5f; node->bbox_max[1] =  0.5f; node->bbox_max[2] =  0.5f;

    // Register as root node initially
#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&ctx->mutex);
#else
    pthread_mutex_lock(&ctx->mutex);
#endif

    if (ctx->root_count >= ctx->root_capacity) {
        uint32_t new_cap = ctx->root_capacity == 0 ? 4 : ctx->root_capacity * 2;
        TessNode** new_roots = (TessNode**)realloc(ctx->root_nodes, new_cap * sizeof(TessNode*));
        if (new_roots) {
            ctx->root_nodes = new_roots;
            ctx->root_capacity = new_cap;
        }
    }

    if (ctx->root_count < ctx->root_capacity) {
        ctx->root_nodes[ctx->root_count++] = node;
    }

#if defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&ctx->mutex);
#else
    pthread_mutex_unlock(&ctx->mutex);
#endif

    return node;
}

int32_t tess_scene_attach_child(TessNode* parent, TessNode* child) {
    uint32_t i;

    if (!parent || !child || parent == child) {
        return -1;
    }

    // Prevent attaching if child already has a parent
    if (child->parent != NULL) {
        return -2;
    }

    if (parent->child_count >= parent->child_capacity) {
        uint32_t new_cap = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        TessNode** new_children = (TessNode**)realloc(parent->children, new_cap * sizeof(TessNode*));
        if (!new_children) {
            return -3;
        }
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }

    parent->children[parent->child_count++] = child;
    child->parent = parent;

    // Update cascading world matrices
    update_world_transforms_recursive(child, &parent->world_transform);

    return 0;
}

// Axis-Aligned Bounding Box (AABB) Ray Intersection Test
static bool ray_intersect_node(const TessNode* node, const float origin[3], const float dir[3], float* out_t) {
    float tmin = 0.0f;
    float tmax = 1e30f;
    int i;

    // Transform bounding box bounds by world matrix translation/scale approximation
    float world_min[3] = {
        node->world_transform.m[12] + node->bbox_min[0],
        node->world_transform.m[13] + node->bbox_min[1],
        node->world_transform.m[14] + node->bbox_min[2]
    };
    float world_max[3] = {
        node->world_transform.m[12] + node->bbox_max[0],
        node->world_transform.m[13] + node->bbox_max[1],
        node->world_transform.m[14] + node->bbox_max[2]
    };

    for (i = 0; i < 3; ++i) {
        float o = origin[i];
        float d = dir[i];
        float min_val = world_min[i];
        float max_val = world_max[i];

        if (fabsf(d) < 1e-6f) {
            if (o < min_val || o > max_val) return false;
        } else {
            float ood = 1.0f / d;
            float t1 = (min_val - o) * ood;
            float t2 = (max_val - o) * ood;

            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }

    if (out_t) *out_t = tmin;
    return true;
}

static bool raycast_recursive(const TessNode* node, const float origin[3], const float dir[3], uint32_t* best_id, float* closest_t) {
    bool hit = false;
    float t = 0.0f;
    uint32_t i;

    if (ray_intersect_node(node, origin, dir, &t)) {
        if (t < *closest_t) {
            *closest_t = t;
            *best_id = node->id;
            hit = true;
        }
    }

    for (i = 0; i < node->child_count; ++i) {
        if (node->children[i]) {
            if (raycast_recursive(node->children[i], origin, dir, best_id, closest_t)) {
                hit = true;
            }
        }
    }

    return hit;
}

int32_t tess_scene_raycast_hit_test(TesseractContext* ctx, const float ray_origin[3], const float ray_dir[3], uint32_t* out_hit_node_id) {
    uint32_t i;
    uint32_t best_id = 0;
    float closest_t = 1e30f;
    bool any_hit = false;

    if (!ctx || !ray_origin || !ray_dir || !out_hit_node_id) {
        return -1;
    }

#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&ctx->mutex);
#else
    pthread_mutex_lock(&ctx->mutex);
#endif

    for (i = 0; i < ctx->root_count; ++i) {
        if (ctx->root_nodes[i]) {
            if (raycast_recursive(ctx->root_nodes[i], ray_origin, ray_dir, &best_id, &closest_t)) {
                any_hit = true;
            }
        }
    }

#if defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&ctx->mutex);
#else
    pthread_mutex_unlock(&ctx->mutex);
#endif

    if (any_hit) {
        *out_hit_node_id = best_id;
        return 0; // Hit found
    }

    *out_hit_node_id = 0;
    return 1; // No hit
}
