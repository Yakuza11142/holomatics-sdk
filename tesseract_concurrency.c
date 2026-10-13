/*
 * File: tesseract_concurrency.c
 * Target: Universal Dynamic Lock-Free Multithreading Engine
 * Constraints: Zero hardcoded strings, zero static buffer limits, dynamic runtime sizing.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <string.h>
#include <stdlib.h>

typedef struct { float m[16]; } TessMatrix4x4;

typedef struct {
    uint32_t command_id;
    float delta_time;
    TessMatrix4x4 payload_matrix;
} TessRenderCommand;

typedef struct {
    TessRenderCommand command;
    _Alignas(64) _Atomic uint32_t sequence;
} TessQueueSlot;

typedef struct {
    TessQueueSlot* buffer;
    size_t capacity;
    size_t bit_mask;
    _Alignas(64) _Atomic uint32_t enqueue_pos;
    _Alignas(64) _Atomic uint32_t dequeue_pos;
} TessAdvancedMPMCQueue;

/**
 * Dynamically instantiates and initializes the MPMC queue with runtime-configured capacity.
 * The capacity is automatically rounded up to the nearest power of two for lightning-fast bitwise masking.
 */
TessAdvancedMPMCQueue* tess_mpmc_queue_create(size_t requested_capacity) {
    if (requested_capacity == 0) return NULL;

    // Find next power of two for universal bitwise masking
    size_t cap = 1;
    while (cap < requested_capacity) {
        cap <<= 1;
    }

    TessAdvancedMPMCQueue* q = (TessAdvancedMPMCQueue*)malloc(sizeof(TessAdvancedMPMCQueue));
    if (!q) return NULL;

    q->buffer = (TessQueueSlot*)aligned_alloc(64, cap * sizeof(TessQueueSlot));
    if (!q->buffer) {
        free(q);
        return NULL;
    }

    q->capacity = cap;
    q->bit_mask = cap - 1;

    for (size_t i = 0; i < cap; ++i) {
        atomic_init(&q->buffer[i].sequence, (uint32_t)i);
        memset(&q->buffer[i].command, 0, sizeof(TessRenderCommand));
    }
    atomic_init(&q->enqueue_pos, 0);
    atomic_init(&q->dequeue_pos, 0);

    return q;
}

/**
 * Universal Thread-Safe Lock-Free Enqueue operation using dynamic bit masking.
 */
bool tess_mpmc_queue_push(TessAdvancedMPMCQueue* q, const TessRenderCommand* cmd) {
    if (!q || !cmd || !q->buffer) return false;

    uint32_t pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
    for (;;) {
        TessQueueSlot* slot = &q->buffer[pos & q->bit_mask];
        uint32_t seq = atomic_load_explicit(&slot->sequence, memory_order_acquire);
        int32_t diff = (int32_t)seq - (int32_t)pos;

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(
                &q->enqueue_pos, &pos, pos + 1,
                memory_order_relaxed, memory_order_relaxed)) {
                slot->command = *cmd;
                atomic_store_explicit(&slot->sequence, pos + 1, memory_order_release);
                return true;
            }
        } else if (diff < 0) {
            return false; // Queue is fully saturated
        } else {
            pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
        }
    }
}

/**
 * Universal Thread-Safe Lock-Free Dequeue operation.
 */
bool tess_mpmc_queue_pop(TessAdvancedMPMCQueue* q, TessRenderCommand* out_cmd) {
    if (!q || !out_cmd || !q->buffer) return false;

    uint32_t pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
    for (;;) {
        TessQueueSlot* slot = &q->buffer[pos & q->bit_mask];
        uint32_t seq = atomic_load_explicit(&slot->sequence, memory_order_acquire);
        int32_t diff = (int32_t)seq - (int32_t)(pos + 1);

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(
                &q->dequeue_pos, &pos, pos + 1,
                memory_order_relaxed, memory_order_relaxed)) {
                *out_cmd = slot->command;
                atomic_store_explicit(&slot->sequence, pos + (uint32_t)q->capacity + 1, memory_order_release);
                return true;
            }
        } else if (diff < 0) {
            return false; // Queue is empty
        } else {
            pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
        }
    }
}

/**
 * Cleanly frees all dynamically allocated memory blocks.
 */
void tess_mpmc_queue_destroy(TessAdvancedMPMCQueue* q) {
    if (q) {
        if (q->buffer) {
            free(q->buffer);
        }
        free(q);
    }
}
