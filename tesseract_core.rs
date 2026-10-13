// Filename: tesseract_core.rs
// Universal High-Performance Spatial Matrix & Vector Engine

use std::ops::Mul;

#[cfg(target_arch = "x86_64")]
use std::arch::x86_64::*;

#[repr(C, align(16))]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Vec4 { pub x: f32, pub y: f32, pub z: f32, pub w: f32 }

impl Vec4 {
    #[inline(always)]
    pub fn new(x: f32, y: f32, z: f32, w: f32) -> Self { Self { x, y, z, w } }
}

#[repr(C, align(16))]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Mat4 { pub cols: [Vec4; 4] }

impl Mat4 {
    #[inline(always)]
    pub fn identity() -> Self {
        Self {
            cols: [
                Vec4::new(1.0, 0.0, 0.0, 0.0),
                Vec4::new(0.0, 1.0, 0.0, 0.0),
                Vec4::new(0.0, 0.0, 1.0, 0.0),
                Vec4::new(0.0, 0.0, 0.0, 1.0),
            ],
        }
    }

    #[inline(always)]
    pub fn multiply(&self, rhs: &Mat4) -> Mat4 {
        #[cfg(target_arch = "x86_64")]
        unsafe {
            let mut result = std::mem::maybe_uninit::<Mat4>::uninit();
            let res_ptr = result.as_mut_ptr() as *mut __m128;
            let lhs0 = _mm_load_ps(&self.cols[0] as *const Vec4 as *const f32);
            let lhs1 = _mm_load_ps(&self.cols[1] as *const Vec4 as *const f32);
            let lhs2 = _mm_load_ps(&self.cols[2] as *const Vec4 as *const f32);
            let lhs3 = _mm_load_ps(&self.cols[3] as *const Vec4 as *const f32);

            for i in 0..4 {
                let rhs_col = _mm_load_ps(&rhs.cols[i] as *const Vec4 as *const f32);
                let xxxx = _mm_shuffle_ps(rhs_col, rhs_col, 0x00);
                let yyyy = _mm_shuffle_ps(rhs_col, rhs_col, 0x55);
                let zzzz = _mm_shuffle_ps(rhs_col, rhs_col, 0xAA);
                let wwww = _mm_shuffle_ps(rhs_col, rhs_col, 0xFF);

                let mut out_col = _mm_mul_ps(lhs0, xxxx);
                out_col = _mm_add_ps(out_col, _mm_mul_ps(lhs1, yyyy));
                out_col = _mm_add_ps(out_col, _mm_mul_ps(lhs2, zzzz));
                out_col = _mm_add_ps(out_col, _mm_mul_ps(lhs3, wwww));
                _mm_store_ps(res_ptr.add(i) as *mut f32, out_col);
            }
            result.assume_init()
        }
        #[cfg(not(target_arch = "x86_64"))]
        {
            let mut res = Mat4::identity();
            for i in 0..4 {
                let c = rhs.cols[i];
                res.cols[i] = Vec4::new(
                    self.cols[0].x * c.x + self.cols[1].x * c.y + self.cols[2].x * c.z + self.cols[3].x * c.w,
                    self.cols[0].y * c.x + self.cols[1].y * c.y + self.cols[2].y * c.z + self.cols[3].y * c.w,
                    self.cols[0].z * c.x + self.cols[1].z * c.y + self.cols[2].z * c.z + self.cols[3].z * c.w,
                    self.cols[0].w * c.x + self.cols[1].w * c.y + self.cols[2].w * c.z + self.cols[3].w * c.w,
                );
            }
            res
        }
    }
}

impl Mul for Mat4 {
    type Output = Self;
    #[inline(always)]
    fn mul(self, rhs: Self) -> Self { self.multiply(&rhs) }
}

#[no_mangle]
pub extern "C" fn tesseract_transform_vector(matrix: *const Mat4, vector: *const Vec4, out: *mut Vec4) {
    if matrix.is_null() || vector.is_null() || out.is_null() { return; }
    unsafe {
        let m = &*matrix;
        let v = &*vector;
        (*out) = Vec4::new(
            m.cols[0].x * v.x + m.cols[1].x * v.y + m.cols[2].x * v.z + m.cols[3].x * v.w,
            m.cols[0].y * v.x + m.cols[1].y * v.y + m.cols[2].y * v.z + m.cols[3].y * v.w,
            m.cols[0].z * v.x + m.cols[1].z * v.y + m.cols[2].z * v.z + m.cols[3].z * v.w,
            m.cols[0].w * v.x + m.cols[1].w * v.y + m.cols[2].w * v.z + m.cols[3].w * v.w,
        );
    }
}
