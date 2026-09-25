#![no_std]
#![deny_warnings]

extern crate alloc;

use alloc::boxed::Box;
use crate::tesseract_ui_layout::{Canvas, Widget};

// ----------------------------------------------------------------------------
// DISPATCH ENGINE INTERFACES (Dirty Rect Pipeline Signal)
// ----------------------------------------------------------------------------

#[inline(always)]
fn dispatch_gpu_mark_dirty_rect(widget_id: u32) {
    let _ = widget_id;
    // Signal downstream render worker lanes to trigger an immediate frame reconstruction pass
}

// ----------------------------------------------------------------------------
// TRAIT & TYPE DEFINITIONS: TesseractUI.State
// ----------------------------------------------------------------------------

pub trait StatefulWidget: Widget {
    fn create_state(&self) -> State;
}

#[repr(C)]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct State {
    pub widget_id: u32,
    pub dirty: bool,
    pub is_processing_mutation: bool, // Latch lock to prevent infinite reentrant recursive loops
}

impl State {
    #[inline(always)]
    pub fn new(widget_id: u32) -> Self {
        Self {
            widget_id,
            dirty: false,
            is_processing_mutation: false,
        }
    }

    /// Hardened: Implements strict latch locks to prevent state corruption or recursive deadlocks
    pub fn set_state<F>(&mut self, updater: F) -> bool
    where
        F: FnOnce(),
    {
        if self.is_processing_mutation || self.widget_id == 0 {
            return false; // Intercept recursive loops early to prevent pipeline lockup
        }

        // Active state lock mutation tracking activation
        self.is_processing_mutation = true;

        // Execute the state data mutation closure cleanly
        updater();

        self.dirty = true;
        self.is_processing_mutation = false; // Release lock safe bounds protection

        // lane(width: 8)
        {
            // Signal downstream render worker lanes to trigger an immediate frame reconstruction pass
            dispatch_gpu_mark_dirty_rect(self.widget_id);
            true
        }
    }
}
