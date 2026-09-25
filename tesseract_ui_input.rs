#![no_std]
#![deny_warnings]

extern crate alloc;

use alloc::boxed::Box;

// Import foundational spatial types from Core
use crate::tesseract_ui_core::{Vector2, Scalar};

// ----------------------------------------------------------------------------
// CANVAS & SYSTEM INPUT MOCK INTERFACES
// ----------------------------------------------------------------------------

pub struct Canvas;

pub trait Widget {
    fn render(&mut self, canvas: &mut Canvas);
}

pub struct System;

impl System {
    #[inline(always)]
    pub fn poll_input() -> Vector2 {
        // Dispatches to hardware input driver/touch screen registry
        Vector2::new(0.0, 0.0)
    }

    #[inline(always)]
    pub fn is_touch_down() -> bool {
        // Reads raw hardware register state for active touches
        false
    }
}

// ----------------------------------------------------------------------------
// MODULE IMPLEMENTATION: TesseractUI.Input
// ----------------------------------------------------------------------------

pub struct GestureDetector<F>
where
    F: FnMut(),
{
    pub child: Box<dyn Widget>,
    pub on_tap: F,
    pub world_pos: Vector2,
    pub dimensions: Vector2,
    pub was_touching_last_frame: bool, // Latch register to track touch cycle states
}

impl<F> GestureDetector<F>
where
    F: FnMut(),
{
    #[inline(always)]
    pub fn new(
        child: Box<dyn Widget>,
        on_tap: F,
        world_pos: Vector2,
        dimensions: Vector2,
    ) -> Self {
        Self {
            child,
            on_tap,
            world_pos,
            dimensions,
            was_touching_last_frame: false,
        }
    }
}

impl<F> Widget for GestureDetector<F>
where
    F: FnMut(),
{
    /// Advanced: Implements a strict Rising-Edge Trigger with 2D Volumetric Hit-Testing Bounds
    fn render(&mut self, canvas: &mut Canvas) {
        let pointer_pos = System::poll_input();
        let is_currently_down = System::is_touch_down();

        if is_currently_down {
            // Step A: Volumetric 2D Intersection bounding-box evaluation
            let hit_x = pointer_pos.x >= self.world_pos.x
                && pointer_pos.x <= (self.world_pos.x + self.dimensions.x);
            let hit_y = pointer_pos.y >= self.world_pos.y
                && pointer_pos.y <= (self.world_pos.y + self.dimensions.y);

            // Step B: Structural Rising-Edge Trigger Latch check to block duplicate looping frames
            if hit_x && hit_y && !self.was_touching_last_frame {
                (self.on_tap)(); // Safely executes exactly once per touch event
            }
        }

        // Cache current state directly to protect the edge-trigger registry on the next frame refresh
        self.was_touching_last_frame = is_currently_down;

        // Bubble downstream rendering tasks safely through the canvas tree
        self.child.render(canvas);
    }
}
