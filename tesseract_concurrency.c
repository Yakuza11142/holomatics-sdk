/*
 * File: tesseract_concurrency.c
 * Target: Universal Dynamic Lock-Free Multithreading Engine (Max Security & 0 Errors)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#endif

// Safe conditional definitions to prevent any redefinition conflicts with tesseract_engine.h
#ifndef TESS_MATRIX4X4_DEFINED
#define TESS_MATRIX4X4_DEFINED
typedef struct {
    float m[16];
} TessMatrix4x4;
#endif

#ifndef TESS_RENDER_COMMAND_DEFINED
#define TESS_RENDER_COMMAND_DEFINED
typedef struct {
    uint32_t command_id;
    float delta_time;
    TessMatrix4x4 payload_matrix;
} TessRenderCommand;
#endif

// Guard against missing alignment macros across different compilers
#ifndef TESS_ALIGNAS
#if defined(_MSC_VER)
#define TESS_ALIGNAS(x) __declspec(align(x))
#elif defined(__GNUC__) || defined(__clang__)
#define TESS_ALIGNAS(x) __attribute__((aligned(x)))
#else
#define TESS_ALIGNAS(x)
#endif
#endif

typedef struct {
    TessRenderCommand command;
    TESS_ALIGNAS(64) atomic_uint sequence;
} TessQueueSlot;

typedef struct {
    TessQueueSlot* buffer;
    size_t capacity;
    size_t bit_mask;
    TESS_ALIGNAS(64) atomic_uint enqueue_pos;
    TESS_ALIGNAS(64) atomic_uint dequeue_pos;
} TessAdvancedMPMCQueue;

static void* tess_aligned_alloc(size_t alignment, size_t size) {
    void* ptr = NULL;
    size_t remainder = size % alignment;
    if (remainder != 0) {
        size += (alignment - remainder);
    }
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
        atomic_store_explicit(&q->buffer[i].sequence, (uint32_t)i, memory_order_relaxed);
        memset(&q->buffer[i].command, 0, sizeof(TessRenderCommand));
    }
    atomic_store_explicit(&q->enqueue_pos, 0, memory_order_relaxed);
    atomic_store_explicit(&q->dequeue_pos, 0, memory_order_relaxed);

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
