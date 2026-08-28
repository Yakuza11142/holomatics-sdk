#include "tesseract_engine.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdalign.h>

#define RING_BUFFER_SIZE 128 
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)

typedef struct {
    uint32_t command_id;
    float delta_time;
    TessMatrix4x4 payload_matrix;
} TessRenderCommand;

typedef struct {
    TessRenderCommand buffer[RING_BUFFER_SIZE];
    alignas(64) atomic_uint head;
    alignas(64) atomic_uint tail;
} TessCommandQueue;

bool tess_queue_push(TessCommandQueue* q, TessRenderCommand cmd) {
    if (q == NULL) return false;

    uint32_t current_tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    uint32_t next_tail = (current_tail + 1) & RING_BUFFER_MASK;

    if (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        return false; 
    }

    q->buffer[current_tail] = cmd;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return true;
}

void tess_process_queued_commands(TesseractContext* ctx, TessCommandQueue* q) {
    if (ctx == NULL || q == NULL) return;

    uint32_t current_head = atomic_load_explicit(&q->head, memory_order_relaxed);

    while (current_head != atomic_load_explicit(&q->tail, memory_order_acquire)) {
        TessRenderCommand cmd = q->buffer[current_head];

        // ---------------------------------------------------------
        // TODO: Mutate engine frame state directly via backend channels
        // tess_apply_render_command(ctx, &cmd);
        // ---------------------------------------------------------

        current_head = (current_head + 1) & RING_BUFFER_MASK;
        atomic_store_explicit(&q->head, current_head, memory_order_release);
    }
}
