pub mod tesseract_infinite_fuzzer {

    // Target spatial math primitives (Mocked/Inlined for completeness)
    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Vector3 {
        pub x: f32,
        pub y: f32,
        pub z: f32,
    }

    impl Vector3 {
        pub const ZERO: Self = Self { x: 0.0, y: 0.0, z: 0.0 };
        
        #[inline]
        pub fn new(x: f32, y: f32, z: f32) -> Self {
            Self { x, y, z }
        }
    }

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub struct Matrix4 {
        pub m: [[f32; 4]; 4],
    }

    impl Matrix4 {
        #[inline]
        pub fn new(
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

    /// Safe pipeline transform target: guards matrix multiplication against NaNs/Infs
    pub fn tess_transform_point_safe(pt: Vector3, mat: Matrix4, fallback: Vector3) -> Vector3 {
        let x = pt.x * mat.m[0][0] + pt.y * mat.m[0][1] + pt.z * mat.m[0][2] + mat.m[0][3];
        let y = pt.x * mat.m[1][0] + pt.y * mat.m[1][1] + pt.z * mat.m[1][2] + mat.m[1][3];
        let z = pt.x * mat.m[2][0] + pt.y * mat.m[2][1] + pt.z * mat.m[2][2] + mat.m[2][3];

        if x.is_nan() || y.is_nan() || z.is_nan() || x.is_infinite() || y.is_infinite() || z.is_infinite() {
            fallback
        } else {
            Vector3::new(x, y, z)
        }
    }

    // ----------------------------------------------------------------------------
    // 1. Data Struct Boundaries
    // ----------------------------------------------------------------------------
    #[derive(Debug, Clone, Copy, Default)]
    pub struct SavedSeed {
        pub seed_val: u32,
        pub hits: u32,
    }

    pub struct FuzzerState {
        pub seed: u32,
        pub total_tests_run: u64,
        pub edge_cases_trapped: u64,
        pub execution_budget_us: f32,

        // Coverage Tracking Map (Simulates 64KB instrumentation bitmap)
        pub coverage_bitmap: [u8; 65536],
        pub unique_features_found: u32,

        // Feedback Queue for interesting mutations
        pub queue: [SavedSeed; 128],
        pub queue_count: usize,
    }

    impl FuzzerState {
        pub fn new(initial_seed: u32) -> Self {
            Self {
                seed: initial_seed,
                total_tests_run: 0,
                edge_cases_trapped: 0,
                execution_budget_us: 0.0,
                coverage_bitmap: [0; 65536],
                unique_features_found: 0,
                queue: [SavedSeed::default(); 128],
                queue_count: 0,
            }
        }
    }

    // ----------------------------------------------------------------------------
    // 2. RANDOMIZATION & DICTIONARY ASSETS
    // ----------------------------------------------------------------------------
    #[inline]
    pub fn tess_next_random(state: &mut FuzzerState) -> u32 {
        state.seed = state.seed.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
        state.seed
    }

    #[inline]
    pub fn tess_get_dictionary_token(state: &mut FuzzerState) -> u32 {
        let idx = tess_next_random(state) % 8;
        match idx {
            0 => 0x7F80_0000, // +Inf
            1 => 0xFF80_0000, // -Inf
            2 => 0x7FC0_0000, // NaN
            3 => 0x0000_0000, // 0.0
            4 => 0x8000_0000, // -0.0
            5 => 0x3F80_0000, // 1.0
            6 => 0xBF80_0000, // -1.0
            _ => 0x0000_0001, // Denormal Min
        }
    }

    // ----------------------------------------------------------------------------
    // 3. BITWISE BIT-FLIPS & DICTIONARY-BASED MUTATIONS
    // ----------------------------------------------------------------------------
    pub fn tess_mutate_bitwise(state: &mut FuzzerState, input_bits: u32) -> f32 {
        let selector = tess_next_random(state) % 4;

        let output_bits = match selector {
            // Strategy 0: Single Bit Flip
            0 => {
                let bit_pos = tess_next_random(state) % 32;
                input_bits ^ (1u32 << bit_pos)
            }
            // Strategy 1: Byte Overwrite (Dictionary Token Injection)
            1 => tess_get_dictionary_token(state),
            // Strategy 2: Multi-Bit Mask Block Blit
            2 => {
                let mask_sel = tess_next_random(state) % 3;
                match mask_sel {
                    0 => input_bits | 0xFF00_0000,
                    1 => input_bits & 0x00FF_FFFF,
                    _ => input_bits ^ 0x5555_5555,
                }
            }
            // Strategy 3: Pure Random Mantissa Noise
            _ => {
                let noise = tess_next_random(state) & 0x00FF_FFFF;
                (input_bits & 0xFF00_0000) | noise
            }
        };

        f32::from_bits(output_bits)
    }

    // ----------------------------------------------------------------------------
    // 4. COVERAGE-GUIDED FEEDBACK TRACKING LOOP
    // ----------------------------------------------------------------------------
    pub fn tess_evaluate_coverage(state: &mut FuzzerState, executed_path_id: u16, working_seed: u32) {
        let map_idx = (executed_path_id as usize) & 0xFFFF;

        if state.coverage_bitmap[map_idx] == 0 {
            state.coverage_bitmap[map_idx] = 1;
            state.unique_features_found += 1;

            if state.queue_count < 128 {
                state.queue[state.queue_count] = SavedSeed {
                    seed_val: working_seed,
                    hits: 0,
                };
                state.queue_count += 1;
            }
        } else if state.coverage_bitmap[map_idx] < 255 {
            state.coverage_bitmap[map_idx] += 1;
        }
    }

    // ----------------------------------------------------------------------------
    // 5. INSTRUMENTED MATRIX PIPELINE FUZZING FLUSH
    // ----------------------------------------------------------------------------
    pub fn tess_fuzz_engine_with_feedback(state: &mut FuzzerState, max_iterations: i32) {
        if max_iterations <= 0 {
            return;
        }

        for _ in 0..max_iterations {
            let mut working_seed = state.seed;

            // Adaptive Input Selection: 40% chance to pull an effective seed from queue
            if state.queue_count > 0 && (tess_next_random(state) % 10) < 4 {
                let q_idx = (tess_next_random(state) as usize) % state.queue_count;
                working_seed = state.queue[q_idx].seed_val;
                state.queue[q_idx].hits += 1;
            }

            // Base structural definitions passing through the mutation engine
            let base_raw = tess_next_random(state);
            let m0 = tess_mutate_bitwise(state, base_raw);
            let next_rand1 = tess_next_random(state);
            let m1 = tess_mutate_bitwise(state, next_rand1);
            let next_rand2 = tess_next_random(state);
            let m2 = tess_mutate_bitwise(state, next_rand2);

            let mat = Matrix4::new(
                m0, m1, m2, 0.0,
                m2, m0, m1, 0.0,
                m1, m2, m0, 0.0,
                0.0, 0.0, 0.0, 1.0,
            );

            let pt = Vector3::new(m0, m1, m2);
            let fallback = Vector3::ZERO;

            let res = tess_transform_point_safe(pt, mat, fallback);

            // Track path and trap anomalies safely using zero-copy reinterpretation
            let res_x_bits = res.x.to_bits();
            let res_y_bits = res.y.to_bits();
            let pseudo_path_hash = ((res_x_bits ^ res_y_bits) & 0xFFFF) as u16;

            tess_evaluate_coverage(state, pseudo_path_hash, working_seed);

            if res.x.is_nan() || res.y.is_nan() || res.z.is_nan()
                || res.x.is_infinite() || res.y.is_infinite() || res.z.is_infinite()
            {
                state.edge_cases_trapped += 1;
            }

            state.total_tests_run += 1;
        }
    }
}
