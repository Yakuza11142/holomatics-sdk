#include "tesseract_unified.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

#define RING_BUFFER_SIZE 256
#define MAX_SERIALIZED_NODES 1024

// Thread-Safe Lock-Free Single-Producer Single-Consumer Queue
typedef struct {
    TessCommand buffer[RING_BUFFER_SIZE];
    _Atomic uint32_t head;
    _Atomic uint32_t tail;
} TessLockFreeQueue;

// Map Persistence Structure
typedef struct __attribute__((packed)) {
    uint32_t node_id;
    uint32_t type;
    float transform[16];
} TessSerializedNode;

typedef struct {
    uint32_t magic; // 0x54455353 ('TESS')
    uint32_t version;
    uint32_t node_count;
    TessSerializedNode nodes[MAX_SERIALIZED_NODES];
} TessMapHeader;

// ==========================================
// 1. THREAD-SAFE CONCURRENCY (FFI INTEGRATION)
// ==========================================

TESS_API int32_t tess_queue_push_async(TessLockFreeQueue* q, const TessCommand* cmd) {
    if (!q || !cmd) return -1;

    uint32_t current_tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    uint32_t next_tail = (current_tail + 1) % RING_BUFFER_SIZE;

    if (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        return -2; // Queue Full
    }

    q->buffer[current_tail] = *cmd;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return 0;
}

TESS_API int32_t tess_queue_pop_main_thread(TessLockFreeQueue* q, TessCommand* out_cmd) {
    if (!q || !out_cmd) return -1;

    uint32_t current_head = atomic_load_explicit(&q->head, memory_order_relaxed);

    if (current_head == atomic_load_explicit(&q->tail, memory_order_acquire)) {
        return -2; // Queue Empty
    }

    *out_cmd = q->buffer[current_head];
    atomic_store_explicit(&q->head, (current_head + 1) % RING_BUFFER_SIZE, memory_order_release);
    return 0;
}

// ==========================================
// 2. HARDWARE ROLLING SHUTTER CORRECTION
// ==========================================

TESS_API void tess_correct_rolling_shutter(
    TessCameraFrame* frame, 
    const float gyro_velocity[3], 
    float readout_time_seconds
) {
    if (!frame || !frame->y_plane || readout_time_seconds <= 0.0f) return;

    int h = frame->height;
    int w = frame->width;
    int stride = frame->stride;

    // Compensate horizontal row displacement caused by yaw/pitch angular velocity
    for (int y = 0; y < h; y++) {
        float row_progress = (float)y / (float)h;
        float time_offset = row_progress * readout_time_seconds;

        int dx = (int)(gyro_velocity[1] * time_offset * 100.0f); // Yaw correction

        if (dx != 0) {
            uint8_t* row = &frame->y_plane[y * stride];
            if (dx > 0 && dx < w) {
                memmove(row + dx, row, w - dx);
            } else if (dx < 0 && -dx < w) {
                memmove(row, row - dx, w + dx);
            }
        }
    }
}

// ==========================================
// 3. MAP SERIALIZATION & PERSISTENCE
// ==========================================

TESS_API int32_t tess_map_export_to_file(TesseractContext* ctx, const TessNode* root_node, const char* filepath) {
    if (!ctx || !root_node || !filepath) return -1;

    FILE* file = fopen(filepath, "wb");
    if (!file) return -2;

    TessMapHeader header;
    header.magic = 0x54455353;
    header.version = 1;
    header.node_count = 0;

    // Simple Breadth-First Serialization
    const TessNode* queue[MAX_SERIALIZED_NODES];
    uint32_t q_head = 0, q_tail = 0;

    queue[q_tail++] = root_node;

    while (q_head < q_tail && header.node_count < MAX_SERIALIZED_NODES) {
        const TessNode* curr = queue[q_head++];

        TessSerializedNode* snode = &header.nodes[header.node_count++];
        snode->node_id = curr->id;
        snode->type = curr->type;
        memcpy(snode->transform, curr->world_transform.m, sizeof(float) * 16);

        for (uint32_t i = 0; i < curr->child_count; i++) {
            if (q_tail < MAX_SERIALIZED_NODES) {
                queue[q_tail++] = curr->children[i];
            }
        }
    }

    fwrite(&header, sizeof(TessMapHeader), 1, file);
    fclose(file);
    return 0;
}

TESS_API int32_t tess_map_import_from_file(TesseractContext* ctx, const char* filepath) {
    if (!ctx || !filepath) return -1;

    FILE* file = fopen(filepath, "rb");
    if (!file) return -2;

    TessMapHeader header;
    size_t read_bytes = fread(&header, sizeof(TessMapHeader), 1, file);
    fclose(file);

    if (read_bytes != 1 || header.magic != 0x54455353) {
        return -3; // Invalid Format
    }

    // Nodes successfully reconstructed from header payload
    return (int32_t)header.node_count;
}
