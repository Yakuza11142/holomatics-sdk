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

typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
} TessMemoryArena;

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

// Function Declarations (Implemented in only ONE compilation unit, e.g., tesseract_ops.c)
TessResult TessArenaInit(TessMemoryArena* arena, size_t capacity);
void* TessArenaAlloc(TessMemoryArena* arena, size_t size, size_t alignment);
void TessArenaReset(TessMemoryArena* arena);
void TessArenaFree(TessMemoryArena* arena);

TessResult TessCorrectRollingShutter(TessCameraFrame* frame, const float gyro_velocity[3], float readout_time_seconds);

void TessQueueInit(TessLockFreeQueue* queue);
TessResult TessQueuePushBatch(TessLockFreeQueue* queue, const TessCommand* cmds, size_t count, size_t* out_pushed);

TessResult TessMapExport(const TessNode* root_node, const char* filepath);
TessResult TessMapImportAndReconstruct(TessMemoryArena* arena, const char* filepath, TessNode** out_root_node);

#endif // TESSERACT_COMMON_H
