// Filename: tesseract_npu_bypass.rs
// Universal Direct-to-Silicon Spatial Hardware Engine (Maximized Speed & Zero-Allocation Hot Loop)

use std::sync::atomic::{AtomicBool, Ordering};
use std::ffi::c_void;

#[repr(C, align(64))]
pub struct HardwareDirectLidarStream {
    pub raw_depth_points: *const f32,
    pub intensity_array: *const u8,
    pub point_capacity: u32,
    pub valid_point_count: u32,
    pub timestamp_ns: u64,
}

#[repr(C, align(16))]
pub struct SphericalHarmonicsLuminance {
    pub bands_red: [f32; 9],
    pub bands_green: [f32; 9],
    pub bands_blue: [f32; 9],
    pub primary_light_direction: [f32; 3],
}

#[repr(C, align(16))]
pub struct SiliconMeshlet {
    pub vertices: *mut [f32; 3],
    pub normals: *mut [f32; 3],
    pub triangle_indices: *mut u8,
    pub vertex_capacity: u16,
    pub index_capacity: u16,
    pub vertex_count: u16,
    pub index_count: u16,
}

pub struct HardwareEngineCore {
    is_dma_locked: AtomicBool,
    cached_lighting: SphericalHarmonicsLuminance,
}

impl HardwareEngineCore {
    #[inline(always)]
    pub const fn new() -> Self {
        Self {
            is_dma_locked: AtomicBool::new(false),
            cached_lighting: SphericalHarmonicsLuminance {
                bands_red: [0.0; 9],
                bands_green: [0.0; 9],
                bands_blue: [0.0; 9],
                primary_light_direction: [0.0, 1.0, 0.0],
            },
        }
    }

    #[inline(always)]
    pub fn compute_instant_hdr_spherical_harmonics(
        &mut self,
        camera_rgba_pixels: &[u8],
        width: usize,
        height: usize,
    ) -> &SphericalHarmonicsLuminance {
        let pixel_count = match width.checked_mul(height) {
            Some(count) if count > 0 => count,
            _ => return &self.cached_lighting,
        };

        if camera_rgba_pixels.is_empty() {
            return &self.cached_lighting;
        }

        let stride = (pixel_count / 256).max(1);
        let mut sum_r = 0.0f32;
        let mut sum_g = 0.0f32;
        let mut sum_b = 0.0f32;
        let mut sample_count = 0.0f32;

        let max_len = camera_rgba_pixels.len().saturating_sub(3);
        let mut idx = 0;

        while idx <= max_len {
            let r = camera_rgba_pixels[idx] as f32 * 0.003921569;
            let g = camera_rgba_pixels[idx + 1] as f32 * 0.003921569;
            let b = camera_rgba_pixels[idx + 2] as f32 * 0.003921569;

            if r.is_finite() && g.is_finite() && b.is_finite() {
                sum_r += r;
                sum_g += g;
                sum_b += b;
                sample_count += 1.0;
            }

            let step = match stride.checked_mul(4) {
                Some(s) => s,
                None => break,
            };

            idx = match idx.checked_add(step) {
                Some(next_idx) => next_idx,
                None => break,
            };
        }

        let inv_samples = 1.0 / sample_count.max(1.0);
        let avg_r = sum_r * inv_samples;
        let avg_g = sum_g * inv_samples;
        let avg_b = sum_b * inv_samples;

        let c0 = 0.282095f32;
        let c1 = 0.488603f32;
        let c3 = 0.315392f32;
        let c4 = 0.546274f32;

        self.cached_lighting.bands_red = [avg_r * c0, 0.0, avg_r * c1, 0.0, 0.0, 0.0, avg_r * c3 * 2.0, 0.0, avg_r * c4];
        self.cached_lighting.bands_green = [avg_g * c0, 0.0, avg_g * c1, 0.0, 0.0, 0.0, avg_g * c3 * 2.0, 0.0, avg_g * c4];
        self.cached_lighting.bands_blue = [avg_b * c0, 0.0, avg_b * c1, 0.0, 0.0, 0.0, avg_b * c3 * 2.0, 0.0, avg_b * c4];

        let total_lum = (avg_r * 0.2126) + (avg_g * 0.7152) + (avg_b * 0.0722);
        let dir_x = (avg_r - total_lum).clamp(-1.0, 1.0);
        let dir_y = (avg_g - total_lum).clamp(-1.0, 1.0).max(0.1);
        let dir_z = (avg_b - total_lum).clamp(-1.0, 1.0);

        let length_sq = (dir_x * dir_x) + (dir_y * dir_y) + (dir_z * dir_z);
        let length_recip = if length_sq.is_finite() && length_sq > 1e-6 { 
            1.0 / length_sq.sqrt() 
        } else { 
            1.0 
        };

        self.cached_lighting.primary_light_direction = [
            dir_x * length_recip,
            dir_y * length_recip,
            dir_z * length_recip,
        ];

        &self.cached_lighting
    }

    #[inline(always)]
    pub fn set_dma_lock(&self, locked: bool) {
        self.is_dma_locked.store(locked, Ordering::Release);
    }

    #[inline(always)]
    pub fn check_dma_lock(&self) -> bool {
        self.is_dma_locked.load(Ordering::Acquire)
    }
}

#[no_mangle]
pub unsafe extern "C" fn tesseract_direct_lidar_mesh(
    lidar_stream: *const HardwareDirectLidarStream,
    out_meshlet: *mut SiliconMeshlet,
) -> bool {
    if lidar_stream.is_null() || out_meshlet.is_null() {
        return false;
    }

    let stream = &*lidar_stream;
    let mesh = &mut *out_meshlet;

    if stream.raw_depth_points.is_null() || mesh.vertices.is_null() || mesh.normals.is_null() || mesh.triangle_indices.is_null() {
        return false;
    }

    let points_to_process = (stream.valid_point_count as usize)
        .min(mesh.vertex_capacity as usize)
        .min(stream.point_capacity as usize);

    if points_to_process < 3 {
        return false;
    }

    let depth_slice = std::slice::from_raw_parts(stream.raw_depth_points, points_to_process * 3);
    let vertex_slice = std::slice::from_raw_parts_mut(mesh.vertices, points_to_process);
    let normal_slice = std::slice::from_raw_parts_mut(mesh.normals, points_to_process);

    for i in 0..points_to_process {
        let base = i * 3;
        vertex_slice[i] = [
            depth_slice[base],
            depth_slice[base + 1],
            depth_slice[base + 2],
        ];
    }

    for i in 0..points_to_process {
        let next_idx = (i + 1) % points_to_process;
        let prev_idx = if i == 0 { points_to_process - 1 } else { i - 1 };

        let v_curr = vertex_slice[i];
        let v_next = vertex_slice[next_idx];
        let v_prev = vertex_slice[prev_idx];

        let edge1 = [v_next[0] - v_curr[0], v_next[1] - v_curr[1], v_next[2] - v_curr[2]];
        let edge2 = [v_prev[0] - v_curr[0], v_prev[1] - v_curr[1], v_prev[2] - v_curr[2]];

        let nx = (edge1[1] * edge2[2]) - (edge1[2] * edge2[1]);
        let ny = (edge1[2] * edge2[0]) - (edge1[0] * edge2[2]);
        let nz = (edge1[0] * edge2[1]) - (edge1[1] * edge2[0]);

        let len_sq = (nx * nx) + (ny * ny) + (nz * nz);
        normal_slice[i] = if len_sq.is_finite() && len_sq > 1e-6 {
            let inv_len = 1.0 / len_sq.sqrt();
            [nx * inv_len, ny * inv_len, nz * inv_len]
        } else {
            [0.0, 1.0, 0.0]
        };
    }

    mesh.vertex_count = points_to_process as u16;

    let index_slice = std::slice::from_raw_parts_mut(mesh.triangle_indices, mesh.index_capacity as usize);
    let mut tri_idx = 0;
    let max_triangles = points_to_process.saturating_sub(2);

    for i in 1..=max_triangles {
        if tri_idx + 2 >= mesh.index_capacity as usize {
            break;
        }
        index_slice[tri_idx] = 0;
        index_slice[tri_idx + 1] = i as u8;
        index_slice[tri_idx + 2] = (i + 1) as u8;
        tri_idx += 3;
    }

    mesh.index_count = tri_idx as u16;
    true
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

unsafe impl Send for TesseractEngine {}
unsafe impl Sync for TesseractEngine {}

impl TesseractEngine {
    pub fn new() -> Result<Self, i32> {
        let ctx = unsafe { tess_create() };
        if ctx.is_null() {
            Err(-2)
        } else {
            Ok(Self { ctx })
        }
    }

    #[inline(always)]
    pub fn update(&mut self, delta_time: f32) -> Result<(), i32> {
        if self.ctx.is_null() { return Err(-2); }
        if !delta_time.is_finite() || delta_time <= 0.0 || delta_time > 1.0 { 
            return Err(-1); 
        }

        let status = unsafe { tess_process_frame(self.ctx, delta_time) };
        if status == 0 { Ok(()) } else { Err(status) }
    }

    #[inline(always)]
    pub fn transform_vector(&self, in_vec: [f32; 3]) -> Result<[f32; 3], i32> {
        if self.ctx.is_null() { return Err(-2); }
        if !in_vec.iter().all(|v| v.is_finite()) {
            return Err(-1);
        }

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
            self.ctx = std::ptr::null_mut();
        }
    }
}
