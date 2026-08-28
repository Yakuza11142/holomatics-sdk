use std::ffi::c_void;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct TessMatrix4x4 {
    pub m: [f32; 16],
}

extern "C" {
    fn tess_create() -> *mut c_void;
    fn tess_destroy(ctx: *mut c_void);
    fn tess_process_frame(ctx: *mut c_void, delta_time: f32) -> i32;
    fn tess_transform_vector(ctx: *mut c_void, in_vec: *const f32, out_vec: *mut f32) -> i32;
}

pub struct TesseractEngine {
    ctx: *mut c_void,
}

// Advanced: Explicitly mark the native context pointer bounds as Send and Sync.
// This allows engineers to pass the context safely between asynchronous thread pools.
unsafe impl Send for TesseractEngine {}
unsafe impl Sync for TesseractEngine {}

impl TesseractEngine {
    pub fn new() -> Result<Self, &'static str> {
        let ctx = unsafe { tess_create() };
        if ctx.is_null() {
            Err("Failed to initialize Tesseract bare-metal runtime context.")
        } else {
            Ok(Self { ctx })
        }
    }

    pub fn update(&self, delta_time: f32) -> Result<(), i32> {
        if delta_time <= 0.0 || delta_time > 1.0 { return Err(-1); }
        let status = unsafe { tess_process_frame(self.ctx, delta_time) };
        if status == 0 { Ok(()) } else { Err(status) }
    }

    pub fn transform_vector(&self, in_vec: [f32; 3]) -> Result<[f32; 3], i32> {
        let mut out_vec = [0.0f32; 3];
        let status = unsafe {
            tess_transform_vector(self.ctx, in_vec.as_ptr(), out_vec.as_mut_ptr())
        };
        if status == 0 { Ok(out_vec) } else { Err(status) }
    }
}

impl Drop for TesseractEngine {
    fn drop(&mut self) {
        if !self.ctx.is_null() {
            unsafe { tess_destroy(self.ctx) };
        }
    }
}
