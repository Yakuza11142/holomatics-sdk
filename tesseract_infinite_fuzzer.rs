pub mod tesseract_infinite_fuzzer {

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Vector3 { pub x: f32, pub y: f32, pub z: f32 }

    impl Vector3 {
        pub const ZERO: Self = Self { x: 0.0, y: 0.0, z: 0.0 };
        #[inline(always)]
        pub const fn new(x: f32, y: f32, z: f32) -> Self { Self { x, y, z } }
    }

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Matrix4 { pub m: [[f32; 4]; 4] }

    impl Matrix4 {
        #[inline(always)]
        pub const fn new(
            m00: f32, m01: f32, m02: f32, m03: f32,
            m10: f32, m11: f32, m12: f32, m13: f32,
            m20: f32, m21: f32, m22: f32, m23: f32,
            m30: f32, m31: f32, m32: f32, m33: f32,
        ) -> Self {
            Self {
                m: [
                    [m00, m01, m02, m03],
                    [m10, m11, m12, m13],
                    [m20, m21, m22, m23],
                    [m30, m31, m32, m33],
                ],
            }
        }
    }

    #[inline(always)]
    pub fn tess_transform_point_safe(pt: Vector3, mat: Matrix4, fallback: Vector3) -> Vector3 {
        let x = pt.x * mat.m[0][0] + pt.y * mat.m[0][1] + pt.z * mat.m[0][2] + mat.m[0][3];
        let y = pt.x * mat.m[1][0] + pt.y * mat.m[1][1] + pt.z * mat.m[1][2] + mat.m[1][3];
        let z = pt.x * mat.m[2][0] + pt.y * mat.m[2][1] + pt.z * mat.m[2][2] + mat.m[2][3];

        if !x.is_finite() || !y.is_finite() || !z.is_finite() {
            fallback
        } else {
            Vector3::new(x, y, z)
        }
    }

    #[derive(Debug, Clone, Copy, Default)]
    pub struct SavedSeed { pub seed_val: u32, pub hits: u32 }

    pub const MAX_QUEUE_SIZE: usize = 128;
    pub const BITMAP_SIZE: usize = 65536;

    pub struct FuzzerState {
        pub seed: u32,
        pub total_tests_run: u64,
        pub edge_cases_trapped: u64,
        pub execution_budget_us: f32,
        pub coverage_bitmap: [u8; BITMAP_SIZE],
        pub unique_features_found: u32,
        pub queue: [SavedSeed; MAX_QUEUE_SIZE],
        pub queue_count: usize,
    }

    impl FuzzerState {
        #[inline(always)]
        pub const fn new(initial_seed: u32) -> Self {
            Self {
                seed: initial_seed,
                total_tests_run: 0,
                edge_cases_trapped: 0,
                execution_budget_us: 0.0,
                coverage_bitmap: [0; BITMAP_SIZE],
                unique_features_found: 0,
                queue: [SavedSeed { seed_val: 0, hits: 0 }; MAX_QUEUE_SIZE],
                queue_count: 0,
            }
        }
    }

    #[inline(always)]
    pub fn tess_next_random(state: &mut FuzzerState) -> u32 {
        state.seed = state.seed.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
        state.seed
    }

    #[inline(always)]
    pub fn tess_evaluate_coverage(state: &mut FuzzerState, executed_path_id: u16, working_seed: u32) {
        let map_idx = (executed_path_id as usize) & (BITMAP_SIZE - 1);

        if state.coverage_bitmap[map_idx] == 0 {
            state.coverage_bitmap[map_idx] = 1;
            state.unique_features_found = state.unique_features_found.saturating_add(1);

            if state.queue_count < MAX_QUEUE_SIZE {
                state.queue[state.queue_count] = SavedSeed { seed_val: working_seed, hits: 0 };
                state.queue_count += 1;
            }
        } else {
            state.coverage_bitmap[map_idx] = state.coverage_bitmap[map_idx].saturating_add(1);
        }
    }

    #[inline(always)]
    pub fn tess_fuzz_engine_with_feedback(state: &mut FuzzerState, max_iterations: i32) {
        if max_iterations <= 0 { return; }

        for _ in 0..max_iterations {
            let mut working_seed = state.seed;

            if state.queue_count > 0 && (tess_next_random(state) % 10) < 4 {
                let q_idx = (tess_next_random(state) as usize) % state.queue_count;
                if q_idx < state.queue_count {
                    working_seed = state.queue[q_idx].seed_val;
                    state.queue[q_idx].hits = state.queue[q_idx].hits.saturating_add(1);
                }
            }

            let m0 = f32::from_bits(tess_next_random(state));
            let m1 = f32::from_bits(tess_next_random(state));
            let m2 = f32::from_bits(tess_next_random(state));

            let mat = Matrix4::new(
                m0, m1, m2, 0.0,
                m2, m0, m1, 0.0,
                m1, m2, m0, 0.0,
                0.0, 0.0, 0.0, 1.0,
            );

            let pt = Vector3::new(m0, m1, m2);
            let res = tess_transform_point_safe(pt, mat, Vector3::ZERO);

            let pseudo_path_hash = ((res.x.to_bits() ^ res.y.to_bits()) & 0xFFFF) as u16;
            tess_evaluate_coverage(state, pseudo_path_hash, working_seed);

            if !res.x.is_finite() || !res.y.is_finite() || !res.z.is_finite() {
                state.edge_cases_trapped = state.edge_cases_trapped.saturating_add(1);
            }

            state.total_tests_run = state.total_tests_run.saturating_add(1);
        }
    }
}
