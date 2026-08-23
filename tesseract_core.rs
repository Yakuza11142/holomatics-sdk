// Filename: tesseract_core.rs
// Tesseract Spatial Matrix Engine - SIMD Matrix Transformations

use std::ops::Mul;

#[repr(C, align(16))]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Vec4 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

impl Vec4 {
    #[inline(always)]
    pub fn new(x: f32, y: f32, z: f32, w: f32) -> Self {
        Self { x, y, z, w }
    }

    #[inline(always)]
    pub fn zero() -> Self {
        Self { x: 0.0, y: 0.0, z: 0.0, w: 0.0 }
    }
}

#[repr(C, align(16))]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Mat4 {
    pub cols: [Vec4; 4],
}

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

    // Fast SIMD-friendly 4x4 matrix multiplication
    #[inline(always)]
    pub fn multiply(&self, rhs: &Mat4) -> Mat4 {
        let mut result = Mat4::identity();
        for i in 0..4 {
            let col = rhs.cols[i];
            result.cols[i] = Vec4::new(
                self.cols[0].x * col.x + self.cols[1].x * col.y + self.cols[2].x * col.z + self.cols[3].x * col.w,
                self.cols[0].y * col.x + self.cols[1].y * col.y + self.cols[2].y * col.z + self.cols[3].y * col.w,
                self.cols[0].z * col.x + self.cols[1].z * col.y + self.cols[2].z * col.z + self.cols[3].z * col.w,
                self.cols[0].w * col.x + self.cols[1].w * col.y + self.cols[2].w * col.z + self.cols[3].w * col.w,
            );
        }
        result
    }

    // Build Floating-Origin Translation Matrix
    #[inline(always)]
    pub fn translation(x: f32, y: f32, z: f32) -> Self {
        let mut m = Self::identity();
        m.cols[3] = Vec4::new(x, y, z, 1.0);
        m
    }

    // Quaternion to 4x4 Rotation Matrix
    #[inline(always)]
    pub fn from_quaternion(q: Vec4) -> Self {
        let (x, y, z, w) = (q.x, q.y, q.z, q.w);
        let (xx, yy, zz) = (x * x, y * y, z * z);
        let (xy, xz, yz) = (x * y, x * z, y * z);
        let (wx, wy, wz) = (w * x, w * y, w * z);

        Self {
            cols: [
                Vec4::new(1.0 - 2.0 * (yy + zz), 2.0 * (xy + wz), 2.0 * (xz - wy), 0.0),
                Vec4::new(2.0 * (xy - wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz + wx), 0.0),
                Vec4::new(2.0 * (xz + wy), 2.0 * (yz - wx), 1.0 - 2.0 * (xx + yy), 0.0),
                Vec4::new(0.0, 0.0, 0.0, 1.0),
            ],
        }
    }
}

// Low-Latency Spatial Transformation Pipeline
#[no_mangle]
pub extern "C" fn tesseract_transform_vector(matrix: *const Mat4, vector: *const Vec4, out: *mut Vec4) {
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
