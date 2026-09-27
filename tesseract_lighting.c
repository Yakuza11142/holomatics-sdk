struct TessCameraIntrinsics {
    fx: f32,
    fy: f32,
    cx: f32,
    cy: f32,
}

struct PipelineUniforms {
    width: u32,
    height: u32,
    stride: u32,
    _pad: u32,
}

struct LightingOutput {
    ambient_intensity: f32,
    color_temp_kelvin: f32,
    light_dir: vec3<f32>,
}

@group(0) @binding(0) var<uniform> cfg: PipelineUniforms;
@group(0) @binding(1) var<storage, read> y_plane_buffer: array<u32>; // Packed 4 pixels per u32 (8-bit per channel)
@group(0) @binding(2) var<storage, read_write> output_data: LightingOutput;

// Shared workgroup memory acting as low-latency register accumulators across local execution lanes
var<workgroup> s_lum: array<f32, 256>;
var<workgroup> s_cx: array<f32, 256>;
var<workgroup> s_cy: array<f32, 256>;

@compute @workgroup_size(16, 16, 1)
fn main(
    @builtin(local_invocation_index) local_idx: u32,
    @builtin(global_invocation_id) global_id: vec3<u32>
) {
    let x = global_id.x;
    let y = global_id.y;

    var pixel_luminosity: f32 = 0.0;
    var local_cx: f32 = 0.0;
    var local_cy: f32 = 0.0;

    // Hardened Structural Guard: Discard execution tracks overshooting the camera boundaries
    if (x < cfg.width && y < cfg.height) {
        let pixel_idx = y * cfg.stride + x;
        let word_idx = pixel_idx / 4u;
        let shift_bits = (pixel_idx % 4u) * 8u;

        // Unpack an individual 8-bit luma channel from the 32-bit packed storage block
        let packed_word = y_plane_buffer[word_idx];
        let val_u8 = (packed_word >> shift_bits) & 0xFFu;
        
        pixel_luminosity = f32(val_u8);
        let weight = pixel_luminosity / 255.0;

        local_cx = f32(x) * weight;
        local_cy = f32(y) * weight;
    }

    // Cache the intermediate pixel outputs straight into localized workgroup shared structures
    s_lum[local_idx] = pixel_luminosity;
    s_cx[local_idx]  = local_cx;
    s_cy[local_idx]  = local_cy;
    workgroupBarrier();

    // High-Speed Tree Reduction Step executing directly inside low-latency registers
    for (var stride: u32 = 128u; stride > 0u; stride >>= 1u) {
        if (local_idx < stride) {
            s_lum[local_idx] += s_lum[local_idx + stride];
            s_cx[local_idx]  += s_cx[local_idx + stride];
            s_cy[local_idx]  += s_cy[local_idx + stride];
        }
        workgroupBarrier();
    }

    // Thread Leader processes the block results and applies the global normalization math
    if (local_idx == 0u) {
        let total_samples = f32(cfg.width * cfg.height);
        
        if (total_samples > 0.0) {
            let global_lum = s_lum[0];
            let total_weight = (global_lum / 255.0);
            
            output_data.ambient_intensity = global_lum / (total_samples * 255.0);
            output_data.color_temp_kelvin = 6500.0 * (output_data.ambient_intensity + 0.5);

            if (total_weight > 0.0) {
                let mean_cx = s_cx[0] / total_weight;
                let mean_cy = s_cy[0] / total_weight;

                let norm_x = (mean_cx - (f32(cfg.width) * 0.5)) / (f32(cfg.width) * 0.5);
                let norm_y = (mean_cy - (f32(cfg.height) * 0.5)) / (f32(cfg.height) * 0.5);
                let radial_sq = min(1.0, (norm_x * norm_x) + (norm_y * norm_y));

                var dir = vec3<f32>(norm_x, -norm_y, 1.0 - sqrt(radial_sq));
                let len = length(dir);
                if (len > 0.0001) {
                    dir = normalize(dir);
                }
                output_data.light_dir = dir;
            } else {
                output_data.light_dir = vec3<f32>(0.0, 1.0, 0.0);
            }
        }
    }
}
