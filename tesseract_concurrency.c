/*
 * File: tesseract_concurrency.c
 * Target: Universal Dynamic Lock-Free Multithreading Engine (Max Security & 0 Errors)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <string.h>
#include <stdlib.h>

// Types are guaranteed to be defined via -include tesseract_engine.h
typedef struct {
    TessRenderCommand command;
    alignas(64) atomic_uint sequence;
} TessQueueSlot;

typedef struct {
    TessQueueSlot* buffer;
    size_t capacity;
    size_t bit_mask;
    alignas(64) atomic_uint enqueue_pos;
    alignas(64) atomic_uint dequeue_pos;
} TessAdvancedMPMCQueue;

static void* tess_aligned_alloc(size_t alignment, size_t size) {
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
}

TessAdvancedMPMCQueue* tess_mpmc_queue_create(size_t requested_capacity) {
    if (requested_capacity == 0 || requested_capacity > (1 << 30)) return NULL;

    size_t cap = 1;
    while (cap < requested_capacity) {
        cap <<= 1;
    }

    TessAdvancedMPMCQueue* q = (TessAdvancedMPMCQueue*)malloc(sizeof(TessAdvancedMPMCQueue));
    if (!q) return NULL;

    q->buffer = (TessQueueSlot*)tess_aligned_alloc(64, cap * sizeof(TessQueueSlot));
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
            return false;
        } else {
            pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
        }
    }
}

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
            return false;
        } else {
            pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
        }
    }
}

void tess_mpmc_queue_destroy(TessAdvancedMPMCQueue* q) {
    if (q) {
        if (q->buffer) {
            free(q->buffer);
        }
        free(q);
    }
}
