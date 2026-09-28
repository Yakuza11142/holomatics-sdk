#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define TESS_RING_BUFFER_SIZE 256
#define TESS_RING_BUFFER_MASK (TESS_RING_BUFFER_SIZE - 1)
#define TESS_MAX_SERIALIZED_NODES 1024
#define TESS_MAGIC_HEADER 0x54455353 // "TESS"
#define TESS_CURRENT_VERSION 3

typedef enum {
    TESS_SUCCESS = 0,
    TESS_ERROR_INVALID_ARGUMENT = -1,
    TESS_ERROR_QUEUE_FULL = -2,
    TESS_ERROR_QUEUE_EMPTY = -3,
    TESS_ERROR_FILE_IO = -4,
    TESS_ERROR_CORRUPTION = -5,
    TESS_ERROR_VERSION_MISMATCH = -6,
    TESS_ERROR_OUT_OF_MEMORY = -7
} TessResult;

// ============================================================================
// 1. MEMORY ARENA ALLOCATOR (Zero-Fragmentation Node Allocation)
// ============================================================================
typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
} TessMemoryArena;

TessResult TessArenaInit(TessMemoryArena* arena, size_t capacity) {
    if (!arena || capacity == 0) return TESS_ERROR_INVALID_ARGUMENT;
    arena->buffer = (uint8_t*)malloc(capacity);
    if (!arena->buffer) return TESS_ERROR_OUT_OF_MEMORY;
    arena->capacity = capacity;
    arena->offset = 0;
    return TESS_SUCCESS;
}

void* TessArenaAlloc(TessMemoryArena* arena, size_t size, size_t alignment) {
    if (!arena || size == 0) return NULL;
    
    if ((alignment & (alignment - 1)) != 0 || alignment == 0) {
        alignment = sizeof(void*);
    }

    size_t current_addr = (size_t)(arena->buffer + arena->offset);
    size_t misalignment = current_addr & (alignment - 1);
    size_t adjustment = misalignment ? (alignment - misalignment) : 0;

    if (arena->offset + adjustment + size > arena->capacity) {
        return NULL; // Out of Arena memory
    }

    arena->offset += adjustment;
    void* ptr = arena->buffer + arena->offset;
    arena->offset += size;
    return ptr;
}

void TessArenaReset(TessMemoryArena* arena) {
    if (arena) arena->offset = 0;
}

void TessArenaFree(TessMemoryArena* arena) {
    if (arena && arena->buffer) {
        free(arena->buffer);
        arena->buffer = NULL;
        arena->capacity = 0;
        arena->offset = 0;
    }
}

// ============================================================================
// 2. DATA STRUCTURES & SPINLOCK-PROTECTED BATCH QUEUE
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

typedef struct {
    _Alignas(64) TessCommand command;
} TessQueueSlot;

typedef struct {
    TessQueueSlot buffer[TESS_RING_BUFFER_SIZE];
    _Alignas(64) atomic_uint head;
    _Alignas(64) atomic_uint tail;
    _Alignas(64) atomic_flag spinlock;
} TessLockFreeQueue;

// ============================================================================
// 3. HARDWARE ROLLING SHUTTER CORRECTION
// ============================================================================
TessResult TessCorrectRollingShutter(TessCameraFrame* frame, const float gyro_velocity[3], float readout_time_seconds) {
    if (!frame || !frame->y_plane || readout_time_seconds <= 0.0f || !gyro_velocity) {
        return TESS_ERROR_INVALID_ARGUMENT;
    }

    int32_t height = frame->height;
    int32_t width = frame->width;
    int32_t stride = frame->stride;

    if (width <= 0 || height <= 0 || stride < width || !isfinite(gyro_velocity[1])) {
        return TESS_ERROR_INVALID_ARGUMENT;
    }

    const float gyro_scale = gyro_velocity[1] * readout_time_seconds * 100.0f / (float)height;

    for (int32_t y = 0; y < height; ++y) {
        int32_t dx = (int32_t)((float)y * gyro_scale);
        if (dx == 0) continue;

        uint8_t* row_ptr = frame->y_plane + (y * stride);

        if (dx > 0 && dx < width) {
            size_t shift = (size_t)dx;
            memmove(row_ptr, row_ptr + shift, width - shift);
            memset(row_ptr + (width - shift), 0, shift);
        } else if (dx < 0 && -dx < width) {
            size_t shift = (size_t)(-dx);
            memmove(row_ptr + shift, row_ptr, width - shift);
            memset(row_ptr, 0, shift);
        }
    }

    return TESS_SUCCESS;
}

// ============================================================================
// 4. CONCURRENCY QUEUE OPERATIONS (Full Push & Pop Batch Support)
// ============================================================================
void TessQueueInit(TessLockFreeQueue* queue) {
    if (!queue) return;
    atomic_init(&queue->head, 0);
    atomic_init(&queue->tail, 0);
    atomic_flag flag_init = ATOMIC_FLAG_INIT;
    queue->spinlock = flag_init;
    memset(queue->buffer, 0, sizeof(queue->buffer));
}

TessResult TessQueuePushBatch(TessLockFreeQueue* queue, const TessCommand* cmds, size_t count, size_t* out_pushed) {
    if (!queue || !cmds || count == 0) return TESS_ERROR_INVALID_ARGUMENT;

    while (atomic_flag_test_and_set_explicit(&queue->spinlock, memory_order_acquire)) {
        // Spin-wait under contention
    }

    uint32_t current_tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    uint32_t current_head = atomic_load_explicit(&queue->head, memory_order_acquire);

    uint32_t available = (current_head > current_tail) ? 
        (current_head - current_tail - 1) : 
        (TESS_RING_BUFFER_SIZE - current_tail + current_head - 1);

    size_t to_push = (count < (size_t)available) ? count : (size_t)available;
    if (to_push == 0) {
        atomic_flag_clear_explicit(&queue->spinlock, memory_order_release);
        if (out_pushed) *out_pushed = 0;
        return TESS_ERROR_QUEUE_FULL;
    }

    uint32_t write_idx = current_tail;
    for (size_t i = 0; i < to_push; ++i) {
        queue->buffer[write_idx].command = cmds[i];
        write_idx = (write_idx + 1) & TESS_RING_BUFFER_MASK;
    }

    atomic_store_explicit(&queue->tail, write_idx, memory_order_release);
    atomic_flag_clear_explicit(&queue->spinlock, memory_order_release);

    if (out_pushed) *out_pushed = to_push;
    return TESS_SUCCESS;
}

TessResult TessQueuePopBatch(TessLockFreeQueue* queue, TessCommand* out_cmds, size_t max_count, size_t* out_popped) {
    if (!queue || !out_cmds || max_count == 0) return TESS_ERROR_INVALID_ARGUMENT;

    while (atomic_flag_test_and_set_explicit(&queue->spinlock, memory_order_acquire)) {
        // Spin-wait under contention
    }

    uint32_t current_head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    uint32_t current_tail = atomic_load_explicit(&queue->tail, memory_order_acquire);

    if (current_head == current_tail) {
        atomic_flag_clear_explicit(&queue->spinlock, memory_order_release);
        if (out_popped) *out_popped = 0;
        return TESS_ERROR_QUEUE_EMPTY;
    }

    uint32_t available = (current_tail >= current_head) ?
        (current_tail - current_head) :
        (TESS_RING_BUFFER_SIZE - current_head + current_tail);

    size_t to_pop = (max_count < (size_t)available) ? max_count : (size_t)available;

    uint32_t read_idx = current_head;
    for (size_t i = 0; i < to_pop; ++i) {
        out_cmds[i] = queue->buffer[read_idx].command;
        read_idx = (read_idx + 1) & TESS_RING_BUFFER_MASK;
    }

    atomic_store_explicit(&queue->head, read_idx, memory_order_release);
    atomic_flag_clear_explicit(&queue->spinlock, memory_order_release);

    if (out_popped) *out_popped = to_pop;
    return TESS_SUCCESS;
}

// ============================================================================
// 5. BULLETPROOF SERIALIZATION & FULL TREE RECONSTRUCTION
// ============================================================================
typedef struct {
    uint32_t node_id;
    uint32_t type;
    uint32_t parent_id;
    float transform[16];
} TessSerializedNode;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t node_count;
    uint32_t checksum;
    TessSerializedNode nodes[TESS_MAX_SERIALIZED_NODES];
} TessSafeMapContainer;

static uint32_t TessComputeChecksum(const uint8_t* data, size_t bytes) {
    if (!data || bytes == 0) return 0;
    uint32_t sum1 = 0xffff, sum2 = 0xffff;
    while (bytes) {
        size_t tlen = (bytes >= 360) ? 360 : bytes;
        bytes -= tlen;
        do {
            sum1 += *data++;
            sum2 += sum1;
        } while (--tlen);
        sum1 = (sum1 & 0xffff) + (sum1 >> 16);
        sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    }
    return ((sum2 & 0xffff) << 16) | (sum1 & 0xffff);
}

TessResult TessMapExport(const TessNode* root_node, const char* filepath) {
    if (!root_node || !filepath) return TESS_ERROR_INVALID_ARGUMENT;

    FILE* file = fopen(filepath, "wb");
    if (!file) return TESS_ERROR_FILE_IO;

    TessSafeMapContainer container;
    memset(&container, 0, sizeof(TessSafeMapContainer));

    container.magic = TESS_MAGIC_HEADER;
    container.version = TESS_CURRENT_VERSION;
    container.node_count = 0;

    typedef struct {
        const TessNode* node;
        uint32_t parent_id;
    } QueueItem;

    QueueItem queue[TESS_MAX_SERIALIZED_NODES];
    size_t q_head = 0;
    size_t q_tail = 0;

    queue[q_tail++] = (QueueItem){root_node, 0xFFFFFFFF};

    while (q_head < q_tail && container.node_count < TESS_MAX_SERIALIZED_NODES) {
        QueueItem item = queue[q_head++];
        if (!item.node) continue;

        size_t idx = container.node_count;
        container.nodes[idx].node_id = item.node->id;
        container.nodes[idx].type = item.node->type;
        container.nodes[idx].parent_id = item.parent_id;
        memcpy(container.nodes[idx].transform, item.node->world_transform.m, sizeof(item.node->world_transform.m));
        container.node_count++;

        for (size_t i = 0; i < item.node->child_count; ++i) {
            if (q_tail < TESS_MAX_SERIALIZED_NODES && item.node->children[i]) {
                queue[q_tail++] = (QueueItem){item.node->children[i], item.node->id};
            }
        }
    }

    size_t payload_size = container.node_count * sizeof(TessSerializedNode);
    container.checksum = TessComputeChecksum((const uint8_t*)container.nodes, payload_size);

    size_t written = fwrite(&container, sizeof(TessSafeMapContainer), 1, file);
    if (ferror(file)) {
        fclose(file);
        return TESS_ERROR_FILE_IO;
    }
    fclose(file);

    return (written == 1) ? TESS_SUCCESS : TESS_ERROR_FILE_IO;
}

TessResult TessMapImportAndReconstruct(TessMemoryArena* arena, const char* filepath, TessNode** out_root_node) {
    if (!arena || !filepath || !out_root_node) return TESS_ERROR_INVALID_ARGUMENT;
    *out_root_node = NULL;

    FILE* file = fopen(filepath, "rb");
    if (!file) return TESS_ERROR_FILE_IO;

    TessSafeMapContainer temp_container;
    memset(&temp_container, 0, sizeof(TessSafeMapContainer));

    size_t read_bytes = fread(&temp_container, sizeof(TessSafeMapContainer), 1, file);
    fclose(file);

    if (read_bytes != 1) return TESS_ERROR_FILE_IO;
    if (temp_container.magic != TESS_MAGIC_HEADER) return TESS_ERROR_CORRUPTION;
    if (temp_container.version != TESS_CURRENT_VERSION) return TESS_ERROR_VERSION_MISMATCH;
    if (temp_container.node_count == 0 || temp_container.node_count > TESS_MAX_SERIALIZED_NODES) {
        return TESS_ERROR_CORRUPTION;
    }

    size_t payload_size = temp_container.node_count * sizeof(TessSerializedNode);
    if (TessComputeChecksum((const uint8_t*)temp_container.nodes, payload_size) != temp_container.checksum) {
        return TESS_ERROR_CORRUPTION;
    }

    // Validate uniqueness of node IDs
    for (uint32_t i = 0; i < temp_container.node_count; ++i) {
        for (uint32_t j = i + 1; j < temp_container.node_count; ++j) {
            if (temp_container.nodes[i].node_id == temp_container.nodes[j].node_id) {
                return TESS_ERROR_CORRUPTION;
            }
        }
    }

    TessSafeMapContainer* container = (TessSafeMapContainer*)TessArenaAlloc(arena, sizeof(TessSafeMapContainer), alignof(TessSafeMapContainer));
    if (!container) return TESS_ERROR_OUT_OF_MEMORY;
    memcpy(container, &temp_container, sizeof(TessSafeMapContainer));

    TessNode** allocated_nodes = (TessNode**)TessArenaAlloc(arena, container->node_count * sizeof(TessNode*), alignof(TessNode*));
    if (!allocated_nodes) return TESS_ERROR_OUT_OF_MEMORY;

    for (uint32_t i = 0; i < container->node_count; ++i) {
        TessNode* node = (TessNode*)TessArenaAlloc(arena, sizeof(TessNode), alignof(TessNode));
        if (!node) return TESS_ERROR_OUT_OF_MEMORY;

        node->id = container->nodes[i].node_id;
        node->type = container->nodes[i].type;
        memcpy(node->world_transform.m, container->nodes[i].transform, sizeof(node->world_transform.m));
        node->children = NULL;
        node->child_count = 0;
        node->child_capacity = 0;

        allocated_nodes[i] = node;
    }

    TessNode* root = NULL;

    // Rebuild tree topology with rigorous, bounded cycle detection
    for (uint32_t i = 0; i < container->node_count; ++i) {
        uint32_t node_id = container->nodes[i].node_id;
        uint32_t parent_id = container->nodes[i].parent_id;
        TessNode* curr = allocated_nodes[i];

        if (parent_id == node_id) {
            return TESS_ERROR_CORRUPTION; // Self-parenting violation
        }

        if (parent_id == 0xFFFFFFFF) {
            if (root != NULL) {
                return TESS_ERROR_CORRUPTION; // Multiple roots detected
            }
            root = curr;
        } else {
            bool parent_found = false;
            for (uint32_t j = 0; j < container->node_count; ++j) {
                if (allocated_nodes[j]->id == parent_id) {
                    TessNode* parent = allocated_nodes[j];
                    
                    // Bounded ancestor cycle verification
                    TessNode* check = parent;
                    uint32_t visit_count = 0;
                    bool has_cycle = false;

                    while (check != NULL && visit_count <= container->node_count) {
                        if (check == curr) {
                            has_cycle = true;
                            break;
                        }
                        uint32_t check_parent_id = 0xFFFFFFFF;
                        for (uint32_t k = 0; k < container->node_count; ++k) {
                            if (allocated_nodes[k] == check) {
                                check_parent_id = container->nodes[k].parent_id;
                                break;
                            }
                        }
                        if (check_parent_id == 0xFFFFFFFF) break;

                        TessNode* next_ancestor = NULL;
                        for (uint32_t k = 0; k < container->node_count; ++k) {
                            if (container->nodes[k].node_id == check_parent_id) {
                                next_ancestor = allocated_nodes[k];
                                break;
                            }
                        }
                        check = next_ancestor;
                        visit_count++;
                    }

                    if (has_cycle || visit_count > container->node_count) {
                        return TESS_ERROR_CORRUPTION;
                    }

                    if (parent->child_count >= parent->child_capacity) {
                        size_t new_cap = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
                        TessNode** new_children = (TessNode**)TessArenaAlloc(arena, new_cap * sizeof(TessNode*), alignof(TessNode*));
                        if (!new_children) return TESS_ERROR_OUT_OF_MEMORY;

                        if (parent->children) {
                            memcpy(new_children, parent->children, parent->child_count * sizeof(TessNode*));
                        }
                        parent->children = new_children;
                        parent->child_capacity = new_cap;
                    }
                    parent->children[parent->child_count++] = curr;
                    parent_found = true;
                    break;
                }
            }
            if (!parent_found) {
                return TESS_ERROR_CORRUPTION; // Orphaned node with missing parent ID
            }
        }
    }

    if (!root) {
        return TESS_ERROR_CORRUPTION;
    }

    *out_root_node = root;
    return TESS_SUCCESS;
}
