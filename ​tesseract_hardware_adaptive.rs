pub mod tesseract_hardware_adaptive {
    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Vector2 { pub x: f32, pub y: f32 }

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Vector3 { pub x: f32, pub y: f32, pub z: f32 }

    impl Vector3 {
        #[inline(always)]
        pub fn sub(self, o: Self) -> Self { Self { x: self.x - o.x, y: self.y - o.y, z: self.z - o.z } }
        #[inline(always)]
        pub fn scale(self, s: f32) -> Self { Self { x: self.x * s, y: self.y * s, z: self.z * s } }
        #[inline(always)]
        pub fn dot(self, o: Self) -> f32 { self.x * o.x + self.y * o.y + self.z * o.z }
        #[inline(always)]
        pub fn cross(self, o: Self) -> Self {
            Self {
                x: self.y * o.z - self.z * o.y,
                y: self.z * o.x - self.x * o.z,
                z: self.x * o.y - self.y * o.x,
            }
        }
    }

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Plane3D { pub nx: f32, pub ny: f32, pub nz: f32, pub d: f32 }
    impl Default for Plane3D { fn default() -> Self { Self { nx: 0.0, ny: 0.0, nz: 1.0, d: 0.0 } } }

    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub struct ColorRGBA { pub r: u8, pub g: u8, pub b: u8, pub a: u8 }

    #[derive(Debug, Clone, Copy, PartialEq, Default)]
    pub struct SHCoeffs { pub r: [f32; 3], pub g: [f32; 3], pub b: [f32; 3] }

    pub fn tess_fit_plane_adaptive(points: &[Vector3], iter: i32, base_thresh: f32, imu_jitter: f32) -> Plane3D {
        let mut best = Plane3D::default();
        let count = points.len();
        if count < 3 || iter <= 0 { return best; }

        let mut max_inliers = 0;
        let dyn_thresh = (base_thresh * (1.0 + (imu_jitter * 2.5).abs())).clamp(0.001, 100.0);
        let mut seed: usize = 0x5EE4;

        for _ in 0..iter {
            seed = seed.wrapping_mul(48271).wrapping_add(1) % 0x7fffffff;
            let i1 = seed % count;
            let i2 = (seed >> 3) % count;
            let i3 = (seed >> 7) % count;
            if i1 == i2 || i2 == i3 || i1 == i3 { continue; }

            let p1 = points[i1];
            let p2 = points[i2];
            let p3 = points[i3];

            let v1 = p2.sub(p1);
            let v2 = p3.sub(p1);
            let norm = v1.cross(v2);
            let len_sq = norm.dot(norm);

            if len_sq > 1e-6 && len_sq.is_finite() {
                let inv_len = 1.0 / len_sq.sqrt();
                let norm = norm.scale(inv_len);
                let d = -norm.dot(p1);
                
                if !d.is_finite() { continue; }

                let inliers = points.iter().filter(|pt| {
                    let val = norm.dot(**pt) + d;
                    val.is_finite() && val.abs() < dyn_thresh
                }).count();

                if inliers > max_inliers {
                    max_inliers = inliers;
                    best = Plane3D { nx: norm.x, ny: norm.y, nz: norm.z, d };
                }
            }
        }
        best
    }

    pub fn tess_estimate_ambient_lighting_thermal(pixels: &[ColorRGBA], w: usize, h: usize, thermal_lvl: i32) -> SHCoeffs {
        let mut sh = SHCoeffs::default();
        let stride = match thermal_lvl { 1 => 32, lvl if lvl >= 2 => 64, _ => 16 };
        if pixels.is_empty() || w <= stride || h <= stride { return sh; }

        let (wf, hf, mut samples) = (w as f32, h as f32, 0_u32);
        let mut y = 0;
        while y < h {
            let mut x = 0;
            while x < w {
                let idx = y.saturating_mul(w).saturating_add(x);
                if idx >= pixels.len() { break; }
                let p = pixels[idx];
                let (r, g, b) = (p.r as f32 * 0.003921569, p.g as f32 * 0.003921569, p.b as f32 * 0.003921569);
                let (nx, ny) = ((x as f32 / wf) * 2.0 - 1.0, (y as f32 / hf) * 2.0 - 1.0);
                let nz = { let ds = nx*nx + ny*ny; if ds < 1.0 { (1.0 - ds).sqrt() } else { 0.0 } };

                sh.r[0] += r * 0.282; sh.r[1] += r * 0.488 * nx; sh.r[2] += r * 0.488 * nz;
                sh.g[0] += g * 0.282; sh.g[1] += g * 0.488 * nx; sh.g[2] += g * 0.488 * nz;
                sh.b[0] += b * 0.282; sh.b[1] += b * 0.488 * nx; sh.b[2] += b * 0.488 * nz;

                samples = samples.saturating_add(1);
                x = x.saturating_add(stride);
            }
            y = y.saturating_add(stride);
        }

        if samples > 0 {
            let sc = 12.56637 / samples as f32;
            for i in 0..3 { 
                sh.r[i] = (sh.r[i] * sc).clamp(-1e5, 1e5); 
                sh.g[i] = (sh.g[i] * sc).clamp(-1e5, 1e5); 
                sh.b[i] = (sh.b[i] * sc).clamp(-1e5, 1e5); 
            }
        }
        sh
    }
}
