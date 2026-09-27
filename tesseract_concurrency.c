#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <string.h>

// Must be a power of 2 for fast bitwise masking operations
#define RING_BUFFER_SIZE 256
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)

typedef struct {
    float m[16];
} TessMatrix4x4;

typedef struct {
    uint32_t command_id;
    float delta_time;
    TessMatrix4x4 payload_matrix;
} TessRenderCommand;

// Hardware Cache Isolation: Pad each element slot to 64 bytes 
// to prevent CPU core cache-line bouncing (False Sharing)
typedef struct {
    TessRenderCommand command;
    _Alignas(64) _Atomic uint32_t sequence;
} TessQueueSlot;

/// Apex Multi-Producer Multi-Consumer (MPMC) Bounded Lock-Free Ring Buffer Engine.
/// Fully safe to share across multi-threaded worker pools in C.
typedef struct {
    _Alignas(64) TessQueueSlot buffer[RING_BUFFER_SIZE];
    _Alignas(64) _Atomic uint32_t enqueue_pos;
    _Alignas(64) _Atomic uint32_t dequeue_pos;
} TessAdvancedMPMCQueue;

/// Instantiates and initializes the ticket-based queue slots.
void tess_mpmc_queue_init(TessAdvancedMPMCQueue* q) {
    for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
        atomic_init(&q->buffer[i].sequence, (uint32_t)i);
        memset(&q->buffer[i].command, 0, sizeof(TessRenderCommand));
    }
    atomic_init(&q->enqueue_pos, 0);
    atomic_init(&q->dequeue_pos, 0);
}

/// Thread-Safe Lock-Free Enqueue operation.
/// Allows multiple threads to push commands concurrently with zero mutex lock-outs.
bool tess_mpmc_queue_push(TessAdvancedMPMCQueue* q, const TessRenderCommand* cmd) {
    uint32_t pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);

    for (;;) {
        TessQueueSlot* slot = &q->buffer[pos & RING_BUFFER_MASK];
        uint32_t seq = atomic_load_explicit(&slot->sequence, memory_order_acquire);
        int32_t diff = (int32_t)seq - (int32_t)pos;

        if (diff == 0) {
            // Try to claim this ticket position
            if (atomic_compare_exchange_weak_explicit(
                &q->enqueue_pos, &pos, pos + 1,
                memory_order_relaxed, memory_order_relaxed)) {
                
                // Safe write: We uniquely claimed this ticket location
                slot->command = *cmd;
                atomic_store_explicit(&slot->sequence, pos + 1, memory_order_release);
                return true;
            }
        } else if (diff < 0) {
            return false; // Queue Is Full
        } else {
            // Another thread moved enqueue_pos; reload it
            pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
        }
    }
}

/// Thread-Safe Lock-Free Dequeue operation.
/// Safely hands off workload commands to whichever worker core is free first.
bool tess_mpmc_queue_pop(TessAdvancedMPMCQueue* q, TessRenderCommand* out_cmd) {
    uint32_t pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);

    for (;;) {
        TessQueueSlot* slot = &q->buffer[pos & RING_BUFFER_MASK];
        uint32_t seq = atomic_load_explicit(&slot->sequence, memory_order_acquire);
        int32_t diff = (int32_t)seq - (int32_t)(pos + 1);

        if (diff == 0) {
            // Try to claim this dequeue position
            if (atomic_compare_exchange_weak_explicit(
                &q->dequeue_pos, &pos, pos + 1,
                memory_order_relaxed, memory_order_relaxed)) {
                
                // Safe read: We uniquely claimed this data slot ticket
                *out_cmd = slot->command;
                atomic_store_explicit(&slot->sequence, pos + RING_BUFFER_SIZE + 1, memory_order_release);
                return true;
            }
        } else if (diff < 0) {
            return false; // Queue is completely empty
        } else {
            // Another thread moved dequeue_pos; reload it
            pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
        }
    }
}
