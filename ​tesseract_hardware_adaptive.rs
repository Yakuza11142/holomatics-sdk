pub mod tesseract_hardware_adaptive {
    use std::cmp::Ordering;

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Vector2 {
        pub x: f32,
        pub y: f32,
    }

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Vector3 {
        pub x: f32,
        pub y: f32,
        pub z: f32,
    }

    impl Vector3 {
        pub const ZERO: Self = Self { x: 0.0, y: 0.0, z: 0.0 };

        #[inline]
        pub fn sub(self, other: Self) -> Self {
            Self {
                x: self.x - other.x,
                y: self.y - other.y,
                z: self.z - other.z,
            }
        }

        #[inline]
        pub fn scale(self, scalar: f32) -> Self {
            Self {
                x: self.x * scalar,
                y: self.y * scalar,
                z: self.z * scalar,
            }
        }

        #[inline]
        pub fn dot(self, other: Self) -> f32 {
            self.x * other.x + self.y * other.y + self.z * other.z
        }

        #[inline]
        pub fn cross(self, other: Self) -> Self {
            Self {
                x: self.y * other.z - self.z * other.y,
                z: self.z * other.x - self.x * other.z,
                y: self.x * other.y - self.y * other.x,
            }
        }
    }

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Plane3D {
        pub nx: f32,
        pub ny: f32,
        pub nz: f32,
        pub d: f32,
    }

    impl Default for Plane3D {
        fn default() -> Self {
            Self { nx: 0.0, ny: 0.0, nz: 1.0, d: 0.0 }
        }
    }

    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub struct ColorRGBA {
        pub r: u8,
        pub g: u8,
        pub b: u8,
        pub a: u8,
    }

    #[derive(Debug, Clone, Copy, PartialEq, Default)]
    pub struct SHCoeffs {
        pub r: [f32; 3],
        pub g: [f32; 3],
        pub b: [f32; 3],
    }

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct DeviceSensorProfile {
        pub imu_variance: f32,
        pub avg_frame_time_ms: f32,
        pub thermal_state: i32, // 0 = Normal, 1 = Warm, 2 = Throttled
        pub auto_cx: f32,
        pub auto_cy: f32,
    }

    // ----------------------------------------------------------------------------
    // 1. HARDENED ADAPTIVE RANSAC (With Dynamic Jitter Tolerances & Loop Guards)
    // ----------------------------------------------------------------------------
    pub fn tess_fit_plane_adaptive(
        points: &[Vector3],
        mut iter: i32,
        base_thresh: f32,
        imu_jitter: f32,
    ) -> Plane3D {
        let mut best_plane = Plane3D::default();
        let count = points.len();

        if count < 3 || iter <= 0 {
            return best_plane;
        }

        let mut max_inliers = 0;
        let mut attempts = 0;
        let max_attempts = iter * 3; // Hard thread cutoff fence

        let jitter_clamp = (imu_jitter * 2.5).clamp(0.0, 5.0);
        let dynamic_thresh = base_thresh * (1.0 + jitter_clamp);

        // Simple XorShift PRNG for zero-dependency fast spatial sampling
        let mut seed = 0x1337_u32;
        let mut next_rand = || -> usize {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            (seed as usize) % count
        };

        while iter > 0 && attempts < max_attempts {
            attempts += 1;

            let idx1 = next_rand();
            let idx2 = next_rand();
            let idx3 = next_rand();

            if idx1 == idx2 || idx2 == idx3 || idx1 == idx3 {
                continue;
            }

            let p1 = points[idx1];
            let p2 = points[idx2];
            let p3 = points[idx3];

            let v1 = p2.sub(p1);
            let v2 = p3.sub(p1);
            let mut norm = v1.cross(v2);
            let len_sq = norm.dot(norm);

            if len_sq > 0.0001 {
                let inv_len = 1.0 / len_sq.sqrt();
                norm = norm.scale(inv_len);
                let d = -norm.dot(p1);

                let mut inliers = 0;
                for pt in points {
                    let dist = (norm.dot(*pt) + d).abs();
                    if dist < dynamic_thresh {
                        inliers += 1;
                    }
                }

                if inliers > max_inliers {
                    max_inliers = inliers;
                    best_plane = Plane3D {
                        nx: norm.x,
                        ny: norm.y,
                        nz: norm.z,
                        d,
                    };
                }

                iter -= 1;
            }
        }

        best_plane
    }

    // ----------------------------------------------------------------------------
    // 2. THERMAL-AWARE AMBIENT LIGHT SH ESTIMATION (With Dynamic Stride Decimation)
    // ----------------------------------------------------------------------------
    pub fn tess_estimate_ambient_lighting_thermal(
        frame_pixels: &[ColorRGBA],
        width: usize,
        height: usize,
        thermal_level: i32,
    ) -> SHCoeffs {
        let mut sh = SHCoeffs::default();

        let step_size = match thermal_level {
            1 => 32,
            lvl if lvl >= 2 => 64,
            _ => 16,
        };

        if frame_pixels.is_empty() || width <= step_size || height <= step_size {
            return sh;
        }

        let w_f = width as f32;
        let h_f = height as f32;
        let mut samples = 0;

        let mut y = 0;
        while y < height {
            let mut x = 0;
            while x < width {
                let idx = y * width + x;
                if idx >= frame_pixels.len() {
                    break;
                }

                let pixel = frame_pixels[idx];
                let r = (pixel.r as f32 * 0.003_921_569).clamp(0.0, 1.0);
                let g = (pixel.g as f32 * 0.003_921_569).clamp(0.0, 1.0);
                let b = (pixel.b as f32 * 0.003_921_569).clamp(0.0, 1.0);

                let nx = (x as f32 / w_f) * 2.0 - 1.0;
                let ny = (y as f32 / h_f) * 2.0 - 1.0;
                let dist_sq = (nx * nx) + (ny * ny);
                let nz = if dist_sq < 1.0 { (1.0 - dist_sq).sqrt() } else { 0.0 };

                let y00 = 0.282_095;
                let y11 = 0.488_602 * nx;
                let y10 = 0.488_602 * nz;

                sh.r[0] += r * y00; sh.r[1] += r * y11; sh.r[2] += r * y10;
                sh.g[0] += g * y00; sh.g[1] += g * y11; sh.g[2] += g * y10;
                sh.b[0] += b * y00; sh.b[1] += b * y11; sh.b[2] += b * y10;

                samples += 1;
                x += step_size;
            }
            y += step_size;
        }

        if samples > 0 {
            let scale = 12.566_371 / (samples as f32); // 4 * PI
            for i in 0..3 {
                sh.r[i] *= scale;
                sh.g[i] *= scale;
                sh.b[i] *= scale;
            }
        }

        sh
    }

    // ----------------------------------------------------------------------------
    // 3. AUTO-CALIBRATING LENS CORRECTION (Iterative Inverse Fixed-Point Solver)
    // ----------------------------------------------------------------------------
    pub fn tess_correct_lens_uncalibrated(
        u: f32,
        v: f32,
        width: f32,
        height: f32,
        k1: f32,
        k2: f32,
    ) -> Vector2 {
        if width <= 0.0 || height <= 0.0 {
            return Vector2 { x: u, y: v };
        }

        let cx = width * 0.5;
        let cy = height * 0.5;
        let fx = width * 0.8;
        let fy = width * 0.8;

        let x_dist = (u - cx) / fx;
        let y_dist = (v - cy) / fy;
        let mut x = x_dist;
        let mut y = y_dist;

        for _ in 0..5 {
            let r2 = (x * x) + (y * y);
            if r2 > 1.5 {
                return Vector2 { x: u, y: v };
            }

            let radial = 1.0 + (k1 * r2) + (k2 * r2 * r2);
            let scale = if radial != 0.0 { radial } else { 1.0 };

            x = x_dist / scale;
            y = y_dist / scale;
        }

        Vector2 {
            x: (x * fx) + cx,
            y: (y * fy) + cy,
        }
    }
}
