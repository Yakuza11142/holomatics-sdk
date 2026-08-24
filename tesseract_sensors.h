#ifndef TESSERACT_SENSORS_H
#define TESSERACT_SENSORS_H

#include "tesseract_engine.h"

// Unified Hardware Camera Frame Struct
typedef struct {
    const uint8_t* y_plane;    // Luminance buffer (Grayscale for SLAM/Tracking)
    const uint8_t* uv_plane;   // Chrominance buffer
    int32_t width;
    int32_t height;
    int32_t stride;
    float camera_intrinsics[9]; // [fx 0 cx, 0 fy cy, 0 0 1]
} TessCameraFrame;

// Unified IMU Hardware Sample
typedef struct {
    float accel[3];    // X, Y, Z linear acceleration
    float gyro[3];     // X, Y, Z angular velocity
    double timestamp;  // Epoch or monotonic camera timestamp
} TessIMUSample;

// Core Engine API Extensions
int32_t tess_push_camera_frame(TesseractContext* ctx, const TessCameraFrame* frame);
int32_t tess_push_imu_sample(TesseractContext* ctx, const TessIMUSample* sample);
int32_t tess_get_pose_estimate(TesseractContext* ctx, TessMatrix4x4* out_pose);

#endif
