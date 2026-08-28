#include "tesseract_engine.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdalign.h> // Required for alignas

// Must be a power of 2 for the bitwise mask to work
#define RING_BUFFER_SIZE 128 
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)

typedef struct {
    uint32_t command_id;
    float delta_time;
    TessMatrix4x4 payload_matrix;
} TessRenderCommand;

typedef struct {
    // 1. Buffer array placed first
    TessRenderCommand buffer[RING_BUFFER_SIZE];
    
    // 2. Align to typical CPU cache line size (64 bytes) to completely eliminate false sharing
    alignas(64) atomic_uint head;
    alignas(64) atomic_uint tail;
} TessCommandQueue;

// Lock-free queue push for a SINGLE background thread (e.g., Camera/Sensor thread)
bool tess_queue_push(TessCommandQueue* q, TessRenderCommand cmd) {
    // Relaxed load is fine here because only this thread mutates 'tail'
    uint32_t current_tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    uint32_t next_tail = (current_tail + 1) & RING_BUFFER_MASK;
    
    // Acquire order ensures we read the most up-to-date head updated by the consumer
    if (next_tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        return false; // Queue full
    }
    
    // Write data to the buffer slot
    q->buffer[current_tail] = cmd;
    
    // Release order guarantees that the command payload write completes 
    // and is visible to other threads BEFORE 'tail' is officially incremented
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return true;
}

// Thread-safe process executed on the single engine render loop thread
void tess_process_queued_commands(TesseractContext* ctx, TessCommandQueue* q) {
    // Relaxed load is fine here because only this thread mutates 'head'
    uint32_t current_head = atomic_load_explicit(&q->head, memory_order_relaxed);
    
    // Acquire order ensures that all payload writes from the producer thread 
    // become visible to this thread before we read the buffer
    while (current_head != atomic_load_explicit(&q->tail, memory_order_acquire)) {
        TessRenderCommand cmd = q->buffer[current_head];
        
        // ---------------------------------------------------------
        // TODO: Safely mutate engine state with your command here:
        // tess_apply_render_command(ctx, &cmd);
        // ---------------------------------------------------------
        
        current_head = (current_head + 1) & RING_BUFFER_MASK;
        
        // Crucial Update: Incrementing head inside the loop with release semantics 
        // allows the producer thread to immediately reclaim slots without waiting 
        // for the entire batch to finish processing.
        atomic_store_explicit(&q->head, current_head, memory_order_release);
    }
}
