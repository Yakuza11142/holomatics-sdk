#include "tesseract_engine.h"
#include <stdatomic.h>
#include <stdbool.h>

#define RING_BUFFER_SIZE 128

typedef struct {
    uint32_t command_id;
    float delta_time;
    TessMatrix4x4 payload_matrix;
} TessRenderCommand;

typedef struct {
    TessRenderCommand buffer[RING_BUFFER_SIZE];
    _Atomic uint32_t head;
    _Atomic uint32_t tail;
} TessCommandQueue;

// Lock-free queue push for background threads (e.g., Camera/Sensor thread)
bool tess_queue_push(TessCommandQueue* q, TessRenderCommand cmd) {
    uint32_t current_tail = atomic_load(&q->tail);
    uint32_t next_tail = (current_tail + 1) % RING_BUFFER_SIZE;
    if (next_tail == atomic_load(&q->head)) {
        return false; // Queue full
    }
    q->buffer[current_tail] = cmd;
    atomic_store(&q->tail, next_tail);
    return true;
}

// Thread-safe process executed safely on the engine render loop
void tess_process_queued_commands(TesseractContext* ctx, TessCommandQueue* q) {
    uint32_t current_head = atomic_load(&q->head);
    while (current_head != atomic_load(&q->tail)) {
        TessRenderCommand cmd = q->buffer[current_head];
        // Safely mutate engine state in a single render thread
        current_head = (current_head + 1) % RING_BUFFER_SIZE;
    }
    atomic_store(&q->head, current_head);
}
