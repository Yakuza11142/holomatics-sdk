#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Const specifications matching your structural engine bounds
#define RING_BUFFER_SIZE 256
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)
#define MAX_SERIALIZED_NODES 1024
#define TESS_MAGIC 0x54455353

// ============================================================================
// 1. DATA STRUCTURES (C-ABI COMPATIBLE)
// ============================================================================

typedef struct {
    uint32_t opcode;
    float payload[4];
} TessCommand;

typedef struct {
    float m[16];
} TessMatrix4x4;

typedef struct TessNode {
    uint32_t id;
    uint32_t type;
    TessMatrix4x4 world_transform;
    struct TessNode** children;
    size_t child_count;
    size_t child_capacity;
} TessNode;

typedef struct {
    uint8_t* y_plane;
    int32_t width;
    int32_t height;
    int32_t stride;
} TessCameraFrame;

// Cache alignment isolation blocks core-to-core false sharing
typedef struct alignas(64) {
    TessCommand command;
} TessQueueSlot;

/// Thread-Safe Lock-Free Single-Producer Single-Consumer Queue Engine (C11 Atomic)
typedef struct {
    TessQueueSlot buffer[RING_BUFFER_SIZE];
    alignas(64) atomic_uint head;
    alignas(64) atomic_uint tail;
} TessLockFreeQueue;

// ============================================================================
// 2. HARDWARE ROLLING SHUTTER CORRECTION
// ============================================================================

void tess_correct_rolling_shutter(TessCameraFrame* frame, const float gyro_velocity[3], float readout_time_seconds) {
    if (!frame || !frame->y_plane || readout_time_seconds <= 0.0f) {
        return;
    }

    int h = frame->height;
    int w = frame->width;
    int stride_val = frame->stride;

    if (w == 0 || h == 0 || stride_val < w) {
        return;
    }

    for (int y = 0; y < h; ++y) {
        float row_progress = (float)y / (float)h;
        float time_offset = row_progress * readout_time_seconds;
        int dx = (int)(gyro_velocity[1] * time_offset * 100.0f);

        if (dx != 0) {
            int row_start = y * stride_val;
            uint8_t* row_ptr = frame->y_plane + row_start;

            if (dx > 0 && dx < w) {
                size_t shift = (size_t)dx;
                memmove(row_ptr, row_ptr + shift, w - shift);
            } else if (dx < 0 && -dx < w) {
                size_t shift = (size_t)(-dx);
                memmove(row_ptr + shift, row_ptr, w - shift);
            }
        }
    }
}

// ============================================================================
// 3. CONCURRENCY QUEUE OPERATIONS
// ============================================================================

void tess_queue_init(TessLockFreeQueue* q) {
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    memset(q->buffer, 0, sizeof(q->buffer));
}

bool tess_queue_push(TessLockFreeQueue* q, const TessCommand* cmd) {
    uint32_t current_tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    uint32_t next_tail = (current_tail + 1) & RING_BUFFER_MASK;

    if (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        return false; // Queue Full
    }

    q->buffer[current_tail].command = *cmd;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return true;
}

bool tess_queue_pop(TessLockFreeQueue* q, TessCommand* out_cmd) {
    uint32_t current_head = atomic_load_explicit(&q->head, memory_order_relaxed);

    if (current_head == atomic_load_explicit(&q->tail, memory_order_acquire)) {
        return false; // Queue Empty
    }

    *out_cmd = q->buffer[current_head].command;
    uint32_t next_head = (current_head + 1) & RING_BUFFER_MASK;
    atomic_store_explicit(&q->head, next_head, memory_order_release);
    return true;
}

// ============================================================================
// 4. MAP SERIALIZATION & PERSISTENCE
// ============================================================================

typedef struct {
    uint32_t node_id;
    uint32_t type;
    float transform[16];
} TessSerializedNode;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t node_count;
    TessSerializedNode nodes[MAX_SERIALIZED_NODES];
} TessMapHeader;

bool tess_map_export(TessMapHeader* header, const TessNode* root_node, const char* filepath) {
    if (!header || !root_node) return false;

    FILE* file = fopen(filepath, "wb");
    if (!file) return false;

    header->magic = TESS_MAGIC;
    header->version = 1;
    header->node_count = 0;

    // Simple BFS queue stack for node traversal
    const TessNode* queue[MAX_SERIALIZED_NODES];
    size_t q_head = 0;
    size_t q_tail = 0;

    queue[q_tail++] = root_node;

    while (q_head < q_tail && header->node_count < MAX_SERIALIZED_NODES) {
        const TessNode* curr = queue[q_head++];
        if (!curr) continue;

        size_t idx = header->node_count;
        header->nodes[idx].node_id = curr->id;
        header->nodes[idx].type = curr->type;
        memcpy(header->nodes[idx].transform, curr->world_transform.m, sizeof(curr->world_transform.m));
        header->node_count++;

        for (size_t i = 0; i < curr->child_count; ++i) {
            if (q_tail < MAX_SERIALIZED_NODES && curr->children[i]) {
                queue[q_tail++] = curr->children[i];
            }
        }
    }

    size_t written = fwrite(header, sizeof(TessMapHeader), 1, file);
    fclose(file);
    return written == 1;
}

bool tess_map_import(TessMapHeader* header, const char* filepath, uint32_t* out_node_count) {
    if (!header || !filepath) return false;

    FILE* file = fopen(filepath, "rb");
    if (!file) return false;

    size_t read_bytes = fread(header, sizeof(TessMapHeader), 1, file);
    fclose(file);

    if (read_bytes != 1 || header->magic != TESS_MAGIC) {
        return false;
    }

    if (out_node_count) {
        *out_node_count = header->node_count;
    }
    return true;
}
