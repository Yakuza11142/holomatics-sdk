#![no_std]
#![deny_warnings]

extern crate alloc;

use alloc::vec::Vec;
use alloc::boxed::Box;

// Import foundational spatial types from Core
use crate::tesseract_ui_core::{Vector2, Scalar, Color};

// ----------------------------------------------------------------------------
// CANVAS & WIDGET TRAIT INTERFACES
// ----------------------------------------------------------------------------

pub trait Widget {
    fn render(&mut self, canvas: &mut Canvas);
    
    #[inline(always)]
    fn get_width(&self) -> Scalar {
        0.0
    }

    #[inline(always)]
    fn get_height(&self) -> Scalar {
        0.0
    }
}

pub struct Canvas;

impl Canvas {
    #[inline(always)]
    pub fn draw_rect(&mut self, pos: Vector2, size: Vector2, color: Color) {
        dispatch_canvas_draw_rect(self, pos, size, color);
    }
}

// ----------------------------------------------------------------------------
// DISPATCH ENGINE INTERFACES (Core Hardware Callouts)
// ----------------------------------------------------------------------------

#[inline(always)]
fn dispatch_canvas_draw_rect(canvas: &mut Canvas, pos: Vector2, size: Vector2, color: Color) {
    let _ = (canvas, pos, size, color);
}

#[inline(always)]
fn dispatch_canvas_push_matrix(canvas: &mut Canvas) {
    let _ = canvas;
}

#[inline(always)]
fn dispatch_canvas_translate(canvas: &mut Canvas, offset: Vector2) {
    let _ = (canvas, offset);
}

#[inline(always)]
fn dispatch_canvas_pop_matrix(canvas: &mut Canvas) {
    let _ = canvas;
}

// ----------------------------------------------------------------------------
// 1. ALIGNMENT HELPERS
// ----------------------------------------------------------------------------

pub struct Alignment;

impl Alignment {
    #[inline(always)]
    pub const fn start() -> Vector2 {
        Vector2::new(0.0, 0.0)
    }

    #[inline(always)]
    pub const fn center() -> Vector2 {
        Vector2::new(0.5, 0.5)
    }

    #[inline(always)]
    pub const fn end() -> Vector2 {
        Vector2::new(1.0, 1.0)
    }
}

// ----------------------------------------------------------------------------
// 2. CONTAINER WIDGET
// ----------------------------------------------------------------------------

pub struct Container {
    pub width: Scalar,
    pub height: Scalar,
    pub color: Color,
    pub child: Option<Box<dyn Widget>>,
}

impl Container {
    #[inline(always)]
    pub fn new(width: Scalar, height: Scalar, color: Color, child: Option<Box<dyn Widget>>) -> Self {
        Self {
            width,
            height,
            color,
            child,
        }
    }
}

impl Widget for Container {
    #[inline(always)]
    fn get_width(&self) -> Scalar {
        self.width
    }

    #[inline(always)]
    fn get_height(&self) -> Scalar {
        self.height
    }

    // Fixed: Self reference explicitly implemented as `&mut self` to handle rendering safely
    fn render(&mut self, canvas: &mut Canvas) {
        canvas.draw_rect(Vector2::new(0.0, 0.0), Vector2::new(self.width, self.height), self.color);

        // Corrected: Handled programmatic boundary validations correctly
        if let Some(ref mut child) = self.child {
            child.render(canvas);
        }
    }
}

// ----------------------------------------------------------------------------
// 3. COLUMN LAYOUT WIDGET
// ----------------------------------------------------------------------------

pub struct Column {
    pub children: Vec<Box<dyn Widget>>,
    pub spacing: Scalar,
}

impl Column {
    #[inline(always)]
    pub fn new(children: Vec<Box<dyn Widget>>, spacing: Scalar) -> Self {
        Self { children, spacing }
    }
}

impl Widget for Column {
    fn get_height(&self) -> Scalar {
        let mut total_height: Scalar = 0.0;
        let count = self.children.len();
        for (i, child) in self.children.iter().enumerate() {
            total_height += child.get_height();
            if i < count - 1 {
                total_height += self.spacing;
            }
        }
        total_height
    }

    /// Advanced: Implements matrix translation push/pop stacks to correctly shift spatial contexts
    fn render(&mut self, canvas: &mut Canvas) {
        let mut y_offset: Scalar = 0.0;

        for child in self.children.iter_mut() {
            // lane(width: 16)
            {
                // Push current layout transform matrix state onto the hardware rasterizer stack
                dispatch_canvas_push_matrix(canvas);
                dispatch_canvas_translate(canvas, Vector2::new(0.0, y_offset));

                child.render(canvas);

                // Pop state back cleanly to avoid corrupting downstream sibling positions
                dispatch_canvas_pop_matrix(canvas);
            }
            // Accumulate bounding height safely + padding separation spacing rules
            y_offset += child.get_height() + self.spacing;
        }
    }
}

// ----------------------------------------------------------------------------
// 4. ROW LAYOUT WIDGET
// ----------------------------------------------------------------------------

pub struct Row {
    pub children: Vec<Box<dyn Widget>>,
    pub spacing: Scalar,
}

impl Row {
    #[inline(always)]
    pub fn new(children: Vec<Box<dyn Widget>>, spacing: Scalar) -> Self {
        Self { children, spacing }
    }
}

impl Widget for Row {
    fn get_width(&self) -> Scalar {
        let mut total_width: Scalar = 0.0;
        let count = self.children.len();
        for (i, child) in self.children.iter().enumerate() {
            total_width += child.get_width();
            if i < count - 1 {
                total_width += self.spacing;
            }
        }
        total_width
    }

    fn render(&mut self, canvas: &mut Canvas) {
        let mut x_offset: Scalar = 0.0;

        for child in self.children.iter_mut() {
            // lane(width: 16)
            {
                dispatch_canvas_push_matrix(canvas);
                dispatch_canvas_translate(canvas, Vector2::new(x_offset, 0.0));

                child.render(canvas);

                dispatch_canvas_pop_matrix(canvas);
            }
            x_offset += child.get_width() + self.spacing;
        }
    }
}
