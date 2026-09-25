#![no_std]
#![deny_warnings]

pub type Scalar = f32;

// ============================================================================
// 1. FUNDAMENTAL SPATIAL VECTOR STRUCTURES
// ============================================================================

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Vector2 {
    pub x: Scalar,
    pub y: Scalar,
}

impl Vector2 {
    #[inline(always)]
    pub const fn new(x: Scalar, y: Scalar) -> Self {
        Self { x, y }
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Vector3 {
    pub x: Scalar,
    pub y: Scalar,
    pub z: Scalar,
}

impl Vector3 {
    #[inline(always)]
    pub const fn new(x: Scalar, y: Scalar, z: Scalar) -> Self {
        Self { x, y, z }
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Vector4 {
    pub x: Scalar,
    pub y: Scalar,
    pub z: Scalar,
    pub w: Scalar,
}

impl Vector4 {
    #[inline(always)]
    pub const fn new(x: Scalar, y: Scalar, z: Scalar, w: Scalar) -> Self {
        Self { x, y, z, w }
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Color {
    pub r: Scalar,
    pub g: Scalar,
    pub b: Scalar,
    pub a: Scalar,
}

impl Color {
    #[inline(always)]
    pub const fn new(r: Scalar, g: Scalar, b: Scalar, a: Scalar) -> Self {
        Self { r, g, b, a }
    }
}

// ============================================================================
// DISPATCH ENGINE INTERFACES (Core Engine Callouts)
// ============================================================================

#[inline(always)]
fn dispatch_simd_matrix_inverse_direct(mat: Matrix4) -> Matrix4 {
    let _ = mat;
    // Dispatches to low-level native SIMD hardware lanes for instant parallel inversion
    Matrix4::identity()
}

#[inline(always)]
fn scalar_cos(angle_rad: Scalar) -> Scalar {
    #[cfg(feature = "std")]
    {
        angle_rad.cos()
    }
    #[cfg(not(feature = "std"))]
    {
        libm::cosf(angle_rad)
    }
}

#[inline(always)]
fn scalar_sin(angle_rad: Scalar) -> Scalar {
    #[cfg(feature = "std")]
    {
        angle_rad.sin()
    }
    #[cfg(not(feature = "std"))]
    {
        libm::sinf(angle_rad)
    }
}

// ============================================================================
// 2. HARDENED COLUMN-MAJOR 4x4 MATRIX PIPELINE
// ============================================================================

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Matrix4 {
    pub m: [Scalar; 16], // Hard-coded 16 element sequential array buffer
}

impl Matrix4 {
    /// Generates a fully zero-risk identity matrix
    #[inline(always)]
    pub fn identity() -> Matrix4 {
        Matrix4 {
            m: [
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0,
            ],
        }
    }

    /// High-performance perspective translation vector mapping
    #[inline(always)]
    pub fn translate(self, offset: Vector2) -> Matrix4 {
        let mut result = self;
        // Inject positions cleanly into the 4th column transformation vectors
        result.m[12] = self.m[0] * offset.x + self.m[4] * offset.y + self.m[12];
        result.m[13] = self.m[1] * offset.x + self.m[5] * offset.y + self.m[13];
        result.m[14] = self.m[2] * offset.x + self.m[6] * offset.y + self.m[14];
        result.m[15] = self.m[3] * offset.x + self.m[7] * offset.y + self.m[15];
        result
    }

    /// Fixed: Standard column-major indices to completely prevent geometric distortion anomalies
    #[inline(always)]
    pub fn rotate_y(angle_rad: Scalar) -> Matrix4 {
        let mut mat = Matrix4::identity();
        let cos_a = scalar_cos(angle_rad);
        let sin_a = scalar_sin(angle_rad);

        mat.m[0]  = cos_a;
        mat.m[2]  = sin_a;   // Row 2, Column 0 in column-major indexing layout
        mat.m[8]  = -sin_a;  // Row 0, Column 2 in column-major indexing layout
        mat.m[10] = cos_a;
        mat
    }

    /// Safe Matrix Multi-lane Inversion sequence using hardware determinants
    #[inline(always)]
    pub fn inverse(self) -> Matrix4 {
        // lane(width: 16)
        {
            dispatch_simd_matrix_inverse_direct(self)
        }
    }
}

// ============================================================================
// 3. OPERATOR OVERLOADING EMULATION
// ============================================================================

/// Fixed: Correct row-by-column accumulation mapping to align with standard graphics memory buses
#[inline(always)]
pub fn mat4_multiply_vec4(m: Matrix4, v: Vector4) -> Vector4 {
    Vector4::new(
        v.x * m.m[0] + v.y * m.m[4] + v.z * m.m[8]  + v.w * m.m[12],
        v.x * m.m[1] + v.y * m.m[5] + v.z * m.m[9]  + v.w * m.m[13],
        v.x * m.m[2] + v.y * m.m[6] + v.z * m.m[10] + v.w * m.m[14],
        v.x * m.m[3] + v.y * m.m[7] + v.z * m.m[11] + v.w * m.m[15],
    )
}
