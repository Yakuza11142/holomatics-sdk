#![no_std]
#![deny_warnings]

// Core type alias mapping Veloct 'scalar' to standard 32-bit float
pub type Scalar = f32;

/// 4x4 Spatial Matrix aligned for SIMD register operations (64 bytes total).
#[repr(C, align(16))]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Matrix4 {
    pub m: [Scalar; 16],
}

impl Matrix4 {
    pub const IDENTITY: Self = Matrix4 {
        m: [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0,
        ],
    };
}

/// Simulated low-level SIMD matrix interpolation function.
/// Maps directly to `dispatch simd_matrix_interpolate(start, end, safe_t)`
#[inline(always)]
pub fn simd_matrix_interpolate(start: Matrix4, end: Matrix4, t: Scalar) -> Matrix4 {
    let mut out = Matrix4 { m: [0.0; 16] };

    // 16-lane vector dispatch loop (compiles directly to SIMD AVX/NEON instructions)
    let mut i = 0;
    while i < 16 {
        out.m[i] = start.m[i] + t * (end.m[i] - start.m[i]);
        i += 1;
    }

    out
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct AnimationController {
    pub duration: Scalar,
    pub progress: Scalar,
}

impl AnimationController {
    /// Fixed: Added explicit self receiver reference to access local variables
    /// Hardened: Clamps interpolation weights strictly to eliminate geometry tearing artifacts
    #[inline(always)]
    pub fn lerp_matrix(self, start: Matrix4, end: Matrix4) -> Matrix4 {
        // Enforce safe saturation limits across interpolation channels
        let safe_t = if self.progress < 0.0 {
            0.0
        } else if self.progress > 1.0 {
            1.0
        } else {
            self.progress
        };

        // lane(width: 16) dispatch execution
        simd_matrix_interpolate(start, end, safe_t)
    }
}
