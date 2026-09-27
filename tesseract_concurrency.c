use std::sync::atomic::{AtomicU32, Ordering};
use std::cell::UnsafeCell;

// Must be a power of 2 for fast bitwise masking operations
const RING_BUFFER_SIZE: usize = 256;
const RING_BUFFER_MASK: usize = RING_BUFFER_SIZE - 1;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct TessMatrix4x4 {
    pub m: [f32; 16],
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct TessRenderCommand {
    pub command_id: u32,
    pub delta_time: f32,
    pub payload_matrix: TessMatrix4x4,
}

// Hardware Cache Isolation: Pad each element slot to 64 bytes 
// to prevent CPU core cache-line bouncing (False Sharing)
#[repr(align(64))]
struct TessQueueSlot {
    command: UnsafeCell<TessRenderCommand>,
    sequence: AtomicU32,
}

/// Apex Multi-Producer Multi-Consumer (MPMC) Bounded Lock-Free Ring Buffer Engine.
/// Fully safe to pass and share across asynchronous threads or Tokio/Rayon thread pools.
pub struct TessAdvancedMPMCQueue {
    buffer: Box<[TessQueueSlot; RING_BUFFER_SIZE]>,
    enqueue_pos: CachePaddedAtomic,
    dequeue_pos: CachePaddedAtomic,
}

#[repr(align(64))]
struct CachePaddedAtomic {
    value: AtomicU32,
}

unsafe impl Send for TessAdvancedMPMCQueue {}
unsafe impl Sync for TessAdvancedMPMCQueue {}

impl TessAdvancedMPMCQueue {
    /// Instantiates and initializes the ticket-based queue slots.
    pub fn new() -> Self {
        // Create an uninitialized array layout securely
        let mut slots = Vec::with_capacity(RING_BUFFER_SIZE);
        for i in 0..RING_BUFFER_SIZE {
            slots.push(TessQueueSlot {
                command: UnsafeCell::new(TessRenderCommand {
                    command_id: 0,
                    delta_time: 0.0,
                    payload_matrix: TessMatrix4x4 { m: [0.0; 16] },
                }),
                sequence: AtomicU32::new(i as u32),
            });
        }
        
        let boxed_buffer: Box<[TessQueueSlot; RING_BUFFER_SIZE]> = match slots.into_boxed_slice().try_into() {
            Ok(b) => b,
            Err(_) => unreachable!(),
        };

        Self {
            buffer: boxed_buffer,
            enqueue_pos: CachePaddedAtomic { value: AtomicU32::new(0) },
            dequeue_pos: CachePaddedAtomic { value: AtomicU32::new(0) },
        }
    }

    /// Thread-Safe Lock-Free Enqueue operation.
    /// Allows hundreds of threads to push commands concurrently with zero mutex lock-outs.
    pub fn push(&self, cmd: TessRenderCommand) -> Result<(), &'static str> {
        let mut pos = self.enqueue_pos.value.load(Ordering::Relaxed);

        loop {
            let slot = &self.buffer[(pos as usize) & RING_BUFFER_MASK];
            let seq = slot.sequence.load(Ordering::Acquire);
            let diff = (seq as i32) - (pos as i32);

            if diff == 0 {
                if self.enqueue_pos.value.compare_exchange_weak(
                    pos, pos + 1, 
                    Ordering::Relaxed, Ordering::Relaxed
                ).is_ok() {
                    // Safe write: We uniquely claimed this ticket location
                    unsafe { *slot.command.get() = cmd; }
                    slot.sequence.store(pos + 1, Ordering::Release);
                    return Ok(());
                }
            } else if diff < 0 {
                return Err("QueueIsFull");
            } else {
                pos = self.enqueue_pos.value.load(Ordering::Relaxed);
            }
        }
    }

    /// Thread-Safe Lock-Free Dequeue operation.
    /// Safely hands off workload commands to whichever worker core is free first.
    pub fn pop(&self) -> Option<TessRenderCommand> {
        let mut pos = self.dequeue_pos.value.load(Ordering::Relaxed);

        loop {
            let slot = &self.buffer[(pos as usize) & RING_BUFFER_MASK];
            let seq = slot.sequence.load(Ordering::Acquire);
            let diff = (seq as i32) - ((pos + 1) as i32);

            if diff == 0 {
                if self.dequeue_pos.value.compare_exchange_weak(
                    pos, pos + 1, 
                    Ordering::Relaxed, Ordering::Relaxed
                ).is_ok() {
                    // Safe read: We uniquely claimed this data slot ticket
                    let cmd = unsafe { *slot.command.get() };
                    slot.sequence.store(pos + (RING_BUFFER_SIZE as u32) + 1, Ordering::Release);
                    return Some(cmd);
                }
            } else if diff < 0 {
                return None; // Queue is completely empty
            } else {
                pos = self.dequeue_pos.value.load(Ordering::Relaxed);
            }
        }
    }
}
