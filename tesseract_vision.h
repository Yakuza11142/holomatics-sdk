#ifndef TESSERACT_VISION_H
#define TESSERACT_VISION_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// SPHERICAL HARMONICS COEFFICIENTS (L1 Band: 4 Coefficients per RGB Channel)
// ----------------------------------------------------------------------------
typedef struct {
    float r[4];
    float g[4];
    float b[4];
} SHCoefficients;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} ColorRGBA;

/**
 * @brief Computes L1 Spherical Harmonics ambient lighting environment map from an RGBA image buffer.
 * 
 * @param frame_ptr Pointer to the start of the 32-bit RGBA pixel buffer.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @return SHCoefficients Computed Spherical Harmonics matrix.
 */
SHCoefficients Tess_EstimateAmbientLighting(const uint8_t* frame_ptr, int32_t width, int32_t height);

#ifdef __cplusplus
}
#endif

#endif // TESSERACT_VISION_H
