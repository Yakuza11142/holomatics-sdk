#ifndef TESSERACT_COMMON_H
#define TESSERACT_COMMON_H

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
// 1. MEMORY ARENA ALLOCATOR
// ============================================================================
typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
} TessMemoryArena;

static inline TessResult TessArenaInit(TessMemoryArena* arena, size_t capacity) {
    if (!arena || capacity == 0) return TESS_ERROR_INVALID_ARGUMENT;
    arena->buffer = (uint8_t*)malloc(capacity);
    if (!arena->buffer) return TESS_ERROR_OUT_OF_MEMORY;
    arena->capacity = capacity;
    arena->offset = 0;
    return TESS_SUCCESS;
}

static inline void* TessArenaAlloc(TessMemoryArena* arena, size_t size, size_t alignment) {
    if (!arena || size == 0) return NULL;
    if ((alignment & (alignment - 1)) != 0 || alignment == 0) {
        alignment = sizeof(void*);
    }
    size_t current_addr = (size_t)(arena->buffer + arena->offset);
    size_t misalignment = current_addr & (alignment - 1);
    size_t adjustment = misalignment ? (alignment - misalignment) : 0;

    if (arena->offset + adjustment + size > arena->capacity) {
        return NULL;
    }
    arena->offset += adjustment;
    void* ptr = arena->buffer + arena->offset;
    arena->offset += size;
    return ptr;
}

static inline void TessArenaReset(TessMemoryArena* arena) {
    if (arena) arena->offset = 0;
}

static inline void TessArenaFree(TessMemoryArena* arena) {
    if (arena && arena->buffer) {
        free(arena->buffer);
        arena->buffer = NULL;
        arena->capacity = 0;
        arena->offset = 0;
    }
}

// ============================================================================
// 2. DATA STRUCTURES & QUEUES
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
} TessLockFreeQueue;

// ============================================================================
// 3. HARDWARE ROLLING SHUTTER CORRECTION
// ============================================================================
static inline TessResult TessCorrectRollingShutter(TessCameraFrame* frame, const float gyro_velocity[3], float readout_time_seconds) {
    if (!frame || !frame->y_plane || readout_time_seconds <= 0.0f) {
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
// 4. CONCURRENCY QUEUE OPERATIONS
// ============================================================================
static inline void TessQueueInit(TessLockFreeQueue* queue) {
    if (!queue) return;
    atomic_init(&queue->head, 0);
    atomic_init(&queue->tail, 0);
    memset(queue->buffer, 0, sizeof(queue->buffer));
}

static inline TessResult TessQueuePushBatch(TessLockFreeQueue* queue, const TessCommand* cmds, size_t count, size_t* out_pushed) {
    if (!queue || !cmds || count == 0) return TESS_ERROR_INVALID_ARGUMENT;

    uint32_t current_tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    uint32_t current_head = atomic_load_explicit(&queue->head, memory_order_acquire);

    uint32_t available = (current_head > current_tail) ? 
        (current_head - current_tail - 1) : 
        (TESS_RING_BUFFER_SIZE - current_tail + current_head - 1);

    size_t to_push = (count < (size_t)available) ? count : (size_t)available;
    if (to_push == 0) {
        if (out_pushed) *out_pushed = 0;
        return TESS_ERROR_QUEUE_FULL;
    }

    for (size_t i = 0; i < to_push; ++i) {
        uint32_t next_tail = (current_tail + 1) & TESS_RING_BUFFER_MASK;
        queue->buffer[current_tail].command = cmds[i];
        current_tail = next_tail;
    }

    atomic_store_explicit(&queue->tail, current_tail, memory_order_release);
    if (out_pushed) *out_pushed = to_push;
    return TESS_SUCCESS;
}

#endif // TESSERACT_COMMON_H
