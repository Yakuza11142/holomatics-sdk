// Filename: tesseract_math.rs
// Universal High-Performance Spatial Math & Linear Algebra Engine

pub mod tesseract_math {

    // ----------------------------------------------------------------------------
    // 0. SPATIAL PRIMITIVES & TYPES
    // ----------------------------------------------------------------------------
    #[derive(Debug, Clone, Copy, PartialEq, Default)]
    pub struct Vector2 {
        pub x: f32,
        pub y: f32,
    }

    impl Vector2 {
        #[inline(always)]
        pub const fn new(x: f32, y: f32) -> Self {
            Self { x, y }
        }
    }

    #[derive(Debug, Clone, Copy, PartialEq, Default)]
    pub struct Vector3 {
        pub x: f32,
        pub y: f32,
        pub z: f32,
    }

    impl Vector3 {
        pub const ZERO: Self = Self { x: 0.0, y: 0.0, z: 0.0 };

        #[inline(always)]
        pub const fn new(x: f32, y: f32, z: f32) -> Self {
            Self { x, y, z }
        }

        #[inline(always)]
        pub fn add(self, rhs: Self) -> Self {
            Self::new(self.x + rhs.x, self.y + rhs.y, self.z + rhs.z)
        }

        #[inline(always)]
        pub fn sub(self, rhs: Self) -> Self {
            Self::new(self.x - rhs.x, self.y - rhs.y, self.z - rhs.z)
        }

        #[inline(always)]
        pub fn scale(self, factor: f32) -> Self {
            Self::new(self.x * factor, self.y * factor, self.z * factor)
        }

        #[inline(always)]
        pub fn dot(self, rhs: Self) -> f32 {
            self.x * rhs.x + self.y * rhs.y + self.z * rhs.z
        }

        #[inline(always)]
        pub fn cross(self, rhs: Self) -> Self {
            Self::new(
                self.y * rhs.z - self.z * rhs.y,
                self.z * rhs.x - self.x * rhs.z,
                self.x * rhs.y - self.y * rhs.x,
            )
        }
    }

    #[derive(Debug, Clone, Copy, PartialEq, Default)]
    pub struct Vector4 {
        pub x: f32,
        pub y: f32,
        pub z: f32,
        pub w: f32,
    }

    impl Vector4 {
        #[inline(always)]
        pub const fn new(x: f32, y: f32, z: f32, w: f32) -> Self {
            Self { x, y, z, w }
        }
    }

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Plane3D {
        pub a: f32,
        pub b: f32,
        pub c: f32,
        pub d: f32,
    }

    impl Plane3D {
        #[inline(always)]
        pub const fn new(a: f32, b: f32, c: f32, d: f32) -> Self {
            Self { a, b, c, d }
        }
    }

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Matrix3 {
        pub m: [[f32; 3]; 3],
    }

    impl Matrix3 {
        pub const IDENTITY: Self = Self {
            m: [
                [1.0, 0.0, 0.0],
                [0.0, 1.0, 0.0],
                [0.0, 0.0, 1.0],
            ],
        };

        pub const ZERO: Self = Self {
            m: [[0.0; 3]; 3],
        };

        #[inline(always)]
        pub fn mul_vec3(&self, v: Vector3) -> Vector3 {
            Vector3::new(
                self.m[0][0] * v.x + self.m[0][1] * v.y + self.m[0][2] * v.z,
                self.m[1][0] * v.x + self.m[1][1] * v.y + self.m[1][2] * v.z,
                self.m[2][0] * v.x + self.m[2][1] * v.y + self.m[2][2] * v.z,
            )
        }

        #[inline(always)]
        pub fn transpose(&self) -> Self {
            Self {
                m: [
                    [self.m[0][0], self.m[1][0], self.m[2][0]],
                    [self.m[0][1], self.m[1][1], self.m[2][1]],
                    [self.m[0][2], self.m[1][2], self.m[2][2]],
                ],
            }
        }

        #[inline(always)]
        pub fn mul(&self, rhs: &Self) -> Self {
            let mut out = Self::ZERO;
            for i in 0..3 {
                for j in 0..3 {
                    out.m[i][j] = self.m[i][0] * rhs.m[0][j]
                        + self.m[i][1] * rhs.m[1][j]
                        + self.m[i][2] * rhs.m[2][j];
                }
            }
            out
        }

        #[inline(always)]
        pub fn determinant(&self) -> f32 {
            self.m[0][0] * (self.m[1][1] * self.m[2][2] - self.m[1][2] * self.m[2][1])
                - self.m[0][1] * (self.m[1][0] * self.m[2][2] - self.m[1][2] * self.m[2][0])
                + self.m[0][2] * (self.m[1][0] * self.m[2][1] - self.m[1][1] * self.m[2][0])
        }

        #[inline(always)]
        pub fn apply_jacobi(&self, i: usize, j: usize, c: f32, s: f32) -> Self {
            let mut out = *self;
            for row in 0..3 {
                let a = out.m[row][i];
                let b = out.m[row][j];
                out.m[row][i] = c * a + s * b;
                out.m[row][j] = -s * a + c * b;
            }
            out
        }
    }

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Matrix4 {
        pub m: [[f32; 4]; 4],
    }

    impl Matrix4 {
        pub const IDENTITY: Self = Self {
            m: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        };

        #[inline(always)]
        pub const fn from_r_t(r: Matrix3, t: Vector3) -> Self {
            Self {
                m: [
                    [r.m[0][0], r.m[0][1], r.m[0][2], t.x],
                    [r.m[1][0], r.m[1][1], r.m[1][2], t.y],
                    [r.m[2][0], r.m[2][1], r.m[2][2], t.z],
                    [0.0, 0.0, 0.0, 1.0],
                ],
            }
        }

        #[inline(always)]
        pub fn mul_vec4(&self, v: Vector4) -> Vector4 {
            Vector4::new(
                self.m[0][0] * v.x + self.m[0][1] * v.y + self.m[0][2] * v.z + self.m[0][3] * v.w,
                self.m[1][0] * v.x + self.m[1][1] * v.y + self.m[1][2] * v.z + self.m[1][3] * v.w,
                self.m[2][0] * v.x + self.m[2][1] * v.y + self.m[2][2] * v.z + self.m[2][3] * v.w,
                self.m[3][0] * v.x + self.m[3][1] * v.y + self.m[3][2] * v.z + self.m[3][3] * v.w,
            )
        }

        #[inline(always)]
        pub fn mul(&self, rhs: &Self) -> Self {
            let mut out = [[0.0; 4]; 4];
            for i in 0..4 {
                for j in 0..4 {
                    out[i][j] = self.m[i][0] * rhs.m[0][j]
                        + self.m[i][1] * rhs.m[1][j]
                        + self.m[i][2] * rhs.m[2][j]
                        + self.m[i][3] * rhs.m[3][j];
                }
            }
            Self { m: out }
        }
    }

    #[inline(always)]
    fn pseudo_random_int(seed: &mut u32, min: usize, max: usize) -> usize {
        if max <= min {
            return min;
        }
        *seed = seed.wrapping_mul(1664525).wrapping_add(1013904223);
        let range = (max - min + 1) as u32;
        min + ((*seed % range) as usize)
    }

    // ----------------------------------------------------------------------------
    // 1. RANSAC, DISTORTION, AND MATRIX GUARDS
    // ----------------------------------------------------------------------------
    #[inline(always)]
    pub fn tess_fit_plane_ransac(points: &[Vector3], iter: i32, thresh: f32) -> Plane3D {
        let mut best_plane = Plane3D::new(0.0, 0.0, 1.0, 0.0);
        let mut max_inliers = 0;
        let mut attempts = 0;
        let max_attempts = iter * 3;
        let count = points.len();
        let mut rng_seed = 0xDEADBEEF;

        if count < 3 {
            return best_plane;
        }

        let mut remaining_iters = iter;

        while remaining_iters > 0 && attempts < max_attempts {
            attempts += 1;

            let idx1 = pseudo_random_int(&mut rng_seed, 0, count - 1);
            let idx2 = pseudo_random_int(&mut rng_seed, 0, count - 1);
            let idx3 = pseudo_random_int(&mut rng_seed, 0, count - 1);

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
                if !inv_len.is_finite() {
                    continue;
                }
                norm = norm.scale(inv_len);
                let d = -norm.dot(p1);
                let mut inliers = 0;

                for pt in points {
                    if (norm.dot(*pt) + d).abs() < thresh {
                        inliers += 1;
                    }
                }

                if inliers > max_inliers {
                    max_inliers = inliers;
                    best_plane = Plane3D::new(norm.x, norm.y, norm.z, d);
                }

                remaining_iters -= 1;
            }
        }

        best_plane
    }

    #[inline(always)]
    pub fn tess_correct_lens_distortion(
        u: f32,
        v: f32,
        fx: f32,
        fy: f32,
        cx: f32,
        cy: f32,
        k1: f32,
        k2: f32,
        p1: f32,
        p2: f32,
    ) -> Vector2 {
        if fx.abs() < 1e-6 || fy.abs() < 1e-6 {
            return Vector2::new(u, v);
        }

        let x_dist = (u - cx) / fx;
        let y_dist = (v - cy) / fy;
        let mut x = x_dist;
        let mut y = y_dist;

        for _ in 0..5 {
            let r2 = (x * x) + (y * y);
            let r4 = r2 * r2;
            let radial = 1.0 + (k1 * r2) + (k2 * r4);
            let tx = 2.0 * p1 * x * y + p2 * (r2 + 2.0 * x * x);
            let ty = p1 * (r2 + 2.0 * y * y) + 2.0 * p2 * x * y;

            x = x_dist - (x * (radial - 1.0) + tx);
            y = y_dist - (y * (radial - 1.0) + ty);
        }

        Vector2::new((x * fx) + cx, (y * fy) + cy)
    }

    #[inline(always)]
    pub fn tess_transform_point_safe(pt: Vector3, mat: Matrix4, fallback_pt: Vector3) -> Vector3 {
        if !pt.x.is_finite() || !pt.y.is_finite() || !pt.z.is_finite() {
            return fallback_pt;
        }

        let res = mat.mul_vec4(Vector4::new(pt.x, pt.y, pt.z, 1.0));

        if !res.x.is_finite()
            || !res.y.is_finite()
            || !res.z.is_finite()
            || !res.w.is_finite()
            || res.w.abs() < 0.00001
        {
            return fallback_pt;
        }

        Vector3::new(res.x, res.y, res.z).scale(1.0 / res.w)
    }

    // ----------------------------------------------------------------------------
    // 2. HARDENED SINGULAR VALUE DECOMPOSITION (SVD 3x3 Engine)
    // ----------------------------------------------------------------------------
    #[inline(always)]
    pub fn tess_solve_svd3x3(mat: &mut Matrix3, out_u: &mut Matrix3, out_v: &mut Matrix3) {
        *out_u = Matrix3::IDENTITY;
        *out_v = Matrix3::IDENTITY;

        for _sweep in 0..6 {
            for i in 0..2 {
                for j in (i + 1)..3 {
                    let p = mat.m[i][j];
                    if p.abs() < 0.00001 {
                        continue;
                    }

                    let h = mat.m[i][i] - mat.m[j][j];
                    let r = (4.0 * p * p + h * h).sqrt();
                    if !r.is_finite() || r < 0.00001 {
                        continue;
                    }

                    let c = ((r + h.abs()) / (2.0 * r)).sqrt();
                    let sign = if h >= 0.0 { 1.0 } else { -1.0 };
                    let s = sign * p / (r * c);

                    *mat = mat.apply_jacobi(i, j, c, s);
                    *out_u = out_u.apply_jacobi(i, j, c, s);
                }
            }
        }
    }

    // ----------------------------------------------------------------------------
    // 3. CACHE-LOCALIZED ITERATIVE CLOSEST POINT (ICP Alignment Pipeline)
    // ----------------------------------------------------------------------------
    #[inline(always)]
    pub fn tess_align_icp(
        src_points: &mut [Vector3],
        dst_points: &[Vector3],
        max_iter: i32,
    ) -> Matrix4 {
        let mut accum_transform = Matrix4::IDENTITY;
        let count = src_points.len();

        if count == 0 || count != dst_points.len() || max_iter <= 0 {
            return accum_transform;
        }

        let scale = 1.0 / (count as f32);

        for _iter in 0..max_iter {
            let mut src_centroid = Vector3::ZERO;
            let mut dst_centroid = Vector3::ZERO;

            for i in 0..count {
                src_centroid = src_centroid.add(src_points[i]);
                dst_centroid = dst_centroid.add(dst_points[i]);
            }

            src_centroid = src_centroid.scale(scale);
            dst_centroid = dst_centroid.scale(scale);

            let mut h_mat = Matrix3::ZERO;
            for i in 0..count {
                let s_norm = src_points[i].sub(src_centroid);
                let d_norm = dst_points[i].sub(dst_centroid);

                h_mat.m[0][0] += s_norm.x * d_norm.x;
                h_mat.m[0][1] += s_norm.x * d_norm.y;
                h_mat.m[0][2] += s_norm.x * d_norm.z;

                h_mat.m[1][0] += s_norm.y * d_norm.x;
                h_mat.m[1][1] += s_norm.y * d_norm.y;
                h_mat.m[1][2] += s_norm.y * d_norm.z;

                h_mat.m[2][0] += s_norm.z * d_norm.x;
                h_mat.m[2][1] += s_norm.z * d_norm.y;
                h_mat.m[2][2] += s_norm.z * d_norm.z;
            }

            let mut u = Matrix3::IDENTITY;
            let mut v = Matrix3::IDENTITY;
            tess_solve_svd3x3(&mut h_mat, &mut u, &mut v);

            let mut r = v.mul(&u.transpose());
            if r.determinant() < 0.0 {
                v.m[0][2] = -v.m[0][2];
                v.m[1][2] = -v.m[1][2];
                v.m[2][2] = -v.m[2][2];
                r = v.mul(&u.transpose());
            }

            let t = dst_centroid.sub(r.mul_vec3(src_centroid));
            let step_transform = Matrix4::from_r_t(r, t);
            accum_transform = step_transform.mul(&accum_transform);

            for i in 0..count {
                src_points[i] = tess_transform_point_safe(
                    src_points[i],
                    step_transform,
                    src_points[i],
                );
            }
        }

        accum_transform
    }
}
