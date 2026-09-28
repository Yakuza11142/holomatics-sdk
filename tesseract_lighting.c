#include "tesseract_common.h"
#include <math.h>

// ============================================================================
// TESSERACT AR LIGHTING & EXPOSURE CORRECTION MODULE
// ============================================================================

typedef struct {
    float ambient_intensity;
    float directional_intensity;
    float color_temperature_kelvin;
    bool is_clipped;
} TessLightingProfile;

/**
 * Compute adaptive lighting and exposure correction based on raw sensor data.
 */
TessResult TessComputeAdaptiveLighting(float raw_luminance, float exposure_bias, TessLightingProfile* out_profile) {
    if (!out_profile || !isfinite(raw_luminance) || !isfinite(exposure_bias)) {
        return TESS_ERROR_INVALID_ARGUMENT;
    }

    // Prevent division by zero or negative luminance
    float clamped_luminance = raw_luminance < 0.001f ? 0.001f : raw_luminance;

    // Apply exposure curve and compensation
    float adjusted_intensity = clamped_luminance * powf(2.0f, exposure_bias);

    out_profile->ambient_intensity = adjusted_intensity * 0.35f;
    out_profile->directional_intensity = adjusted_intensity * 0.65f;
    out_profile->color_temperature_kelvin = 6500.0f; // Standard daylight baseline

    // Check for sensor clipping (over-exposure / blown highlights)
    out_profile->is_clipped = (adjusted_intensity > 1.0f);

    return TESS_SUCCESS;
}

/**
 * Blend lighting profiles smoothly across dynamic AR frames using exponential moving average.
 */
TessResult TessBlendLightingProfiles(const TessLightingProfile* current, const TessLightingProfile* target, float alpha, TessLightingProfile* out_result) {
    if (!current || !target || !out_result || alpha < 0.0f || alpha > 1.0f) {
        return TESS_ERROR_INVALID_ARGUMENT;
    }

    out_result->ambient_intensity = current->ambient_intensity * (1.0f - alpha) + target->ambient_intensity * alpha;
    out_result->directional_intensity = current->directional_intensity * (1.0f - alpha) + target->directional_intensity * alpha;
    out_result->color_temperature_kelvin = current->color_temperature_kelvin * (1.0f - alpha) + target->color_temperature_kelvin * alpha;
    out_result->is_clipped = target->is_clipped;

    return TESS_SUCCESS;
}
