// Filename: tesseract_npu_bypass.rs
// Tesseract Direct-to-Silicon Spatial Hardware Engine

use std::sync::atomic::{AtomicBool, Ordering};

#[repr(C, align(64))]
pub struct HardwareDirectLidarStream {
    pub raw_depth_points: [f32; 4096],
    pub intensity_array: [u8; 4096],
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
    pub vertices: [[f32; 3]; 64],
    pub normals: [[f32; 3]; 64],
    pub triangle_indices: [u8; 126],
    pub vertex_count: u8,
    pub index_count: u8,
}

pub struct HardwareEngineCore {
    is_dma_locked: AtomicBool,
    cached_lighting: SphericalHarmonicsLuminance,
}

impl HardwareEngineCore {
    pub fn new() -> Self {
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
        let pixel_count = width * height;
        if pixel_count == 0 || camera_rgba_pixels.is_empty() {
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

            sum_r += r;
            sum_g += g;
            sum_b += b;
            sample_count += 1.0;

            idx = match idx.checked_add(stride * 4) {
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
        let c2 = 1.092548f32;
        let c3 = 0.315392f32;
        let c4 = 0.546274f32;

        self.cached_lighting.bands_red[0] = avg_r * c0;
        self.cached_lighting.bands_red[1] = avg_r * c1 * 0.0;
        self.cached_lighting.bands_red[2] = avg_r * c1 * 1.0;
        self.cached_lighting.bands_red[3] = avg_r * c1 * 0.0;
        self.cached_lighting.bands_red[4] = avg_r * c2 * 0.0;
        self.cached_lighting.bands_red[5] = avg_r * c2 * 0.0;
        self.cached_lighting.bands_red[6] = avg_r * c3 * (3.0 * 1.0 - 1.0);
        self.cached_lighting.bands_red[7] = avg_r * c2 * 0.0;
        self.cached_lighting.bands_red[8] = avg_r * c4 * 1.0;

        self.cached_lighting.bands_green[0] = avg_g * c0;
        self.cached_lighting.bands_green[1] = avg_g * c1 * 0.0;
        self.cached_lighting.bands_green[2] = avg_g * c1 * 1.0;
        self.cached_lighting.bands_green[3] = avg_g * c1 * 0.0;
        self.cached_lighting.bands_green[4] = avg_g * c2 * 0.0;
        self.cached_lighting.bands_green[5] = avg_g * c2 * 0.0;
        self.cached_lighting.bands_green[6] = avg_g * c3 * (3.0 * 1.0 - 1.0);
        self.cached_lighting.bands_green[7] = avg_g * c2 * 0.0;
        self.cached_lighting.bands_green[8] = avg_g * c4 * 1.0;

        self.cached_lighting.bands_blue[0] = avg_b * c0;
        self.cached_lighting.bands_blue[1] = avg_b * c1 * 0.0;
        self.cached_lighting.bands_blue[2] = avg_b * c1 * 1.0;
        self.cached_lighting.bands_blue[3] = avg_b * c1 * 0.0;
        self.cached_lighting.bands_blue[4] = avg_b * c2 * 0.0;
        self.cached_lighting.bands_blue[5] = avg_b * c2 * 0.0;
        self.cached_lighting.bands_blue[6] = avg_b * c3 * (3.0 * 1.0 - 1.0);
        self.cached_lighting.bands_blue[7] = avg_b * c2 * 0.0;
        self.cached_lighting.bands_blue[8] = avg_b * c4 * 1.0;

        let luminance_r = avg_r * 0.2126;
        let luminance_g = avg_g * 0.7152;
        let luminance_b = avg_b * 0.0722;
        let total_lum = luminance_r + luminance_g + luminance_b;

        let dir_x = (avg_r - total_lum).clamp(-1.0, 1.0);
        let dir_y = (avg_g - total_lum).clamp(-1.0, 1.0).max(0.1);
        let dir_z = (avg_b - total_lum).clamp(-1.0, 1.0);

        let length_recip = 1.0 / (dir_x * dir_x + dir_y * dir_y + dir_z * dir_z).sqrt();
        self.cached_lighting.primary_light_direction = [
            dir_x * length_recip,
            dir_y * length_recip,
            dir_z * length_recip,
        ];

        &self.cached_lighting
    }

    #[no_mangle]
    pub extern "C" fn tesseract_direct_lidar_mesh(
        lidar_stream: *const HardwareDirectLidarStream,
        out_meshlet: *mut SiliconMeshlet,
    ) -> bool {
        if lidar_stream.is_null() || out_meshlet.is_null() {
            return false;
        }

        unsafe {
            let stream = &*lidar_stream;
            let mesh = &mut *out_meshlet;

            let points_to_process = (stream.valid_point_count as usize).min(64);
            if points_to_process < 3 {
                return false;
            }

            for i in 0..points_to_process {
                let base = i * 3;
                if base + 2 >= stream.raw_depth_points.len() {
                    return false;
                }

                let px = stream.raw_depth_points[base];
                let py = stream.raw_depth_points[base + 1];
                let pz = stream.raw_depth_points[base + 2];

                mesh.vertices[i] = [px, py, pz];
            }

            for i in 0..points_to_process {
                let current_idx = i;
                let next_idx = (i + 1) % points_to_process;
                let prev_idx = if i == 0 { points_to_process - 1 } else { i - 1 };

                let v_curr = mesh.vertices[current_idx];
                let v_next = mesh.vertices[next_idx];
                let v_prev = mesh.vertices[prev_idx];

                let edge1 = [
                    v_next[0] - v_curr[0],
                    v_next[1] - v_curr[1],
                    v_next[2] - v_curr[2],
                ];
                let edge2 = [
                    v_prev[0] - v_curr[0],
                    v_prev[1] - v_curr[1],
                    v_prev[2] - v_curr[2],
                ];

                let nx = edge1[1] * edge2[2] - edge1[2] * edge2[1];
                let ny = edge1[2] * edge2[0] - edge1[0] * edge2[2];
                let nz = edge1[0] * edge2[1] - edge1[1] * edge2[0];

                let len_sq = nx * nx + ny * ny + nz * nz;
                if len_sq > 1e-6 {
                    let inv_len = 1.0 / len_sq.sqrt();
                    mesh.normals[i] = [nx * inv_len, ny * inv_len, nz * inv_len];
                } else {
                    mesh.normals[i] = [0.0, 1.0, 0.0];
                }
            }

            mesh.vertex_count = points_to_process as u8;

            let mut tri_idx = 0;
            let max_triangles = points_to_process.saturating_sub(2);
            for i in 1..=max_triangles {
                if tri_idx + 2 >= mesh.triangle_indices.len() {
                    break;
                }
                mesh.triangle_indices[tri_idx] = 0;
                mesh.triangle_indices[tri_idx + 1] = i as u8;
                mesh.triangle_indices[tri_idx + 2] = (i + 1) as u8;
                tri_idx += 3;
            }

            mesh.index_count = tri_idx as u8;
        }

        true
    }
}
