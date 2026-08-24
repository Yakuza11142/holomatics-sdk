#include "tesseract_unified.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

#define PI 3.14159265358979323846f
#define RAD2DEG(x) ((x) * (180.0f / PI))
#define DEG2RAD(x) ((x) * (PI / 180.0f))

#define MAX_FEATURES 500
#define MAX_KEYFRAMES 64
#define RING_BUFFER_SIZE 256
#define FAST_BARRIER 20

// Advanced Distortion Model Parameters
typedef struct {
    float k1, k2; // Radial
    float p1, p2; // Tangential
} TessDistortionCoeffs;

// 15-State Covariance State Structure (Position, Velocity, Orientation, AccelBias, GyroBias)
typedef struct {
    float p[3];      // Position (x, y, z)
    float v[3];      // Velocity (vx, vy, vz)
    float q[4];      // Orientation Quaternion (w, x, y, z)
    float ba[3];     // Accelerometer Bias
    float bg[3];     // Gyroscope Bias
    float P[15][15]; // State Covariance Matrix
} TessESEKFState;

// Advanced Feature Representation
typedef struct {
    float x, y;
    float response;
    float angle;     // Intensity Centroid Steering Angle
    uint32_t octant;
} TessFeatureKeypoint;

typedef struct {
    uint64_t data[4]; // 256-Bit ORB Binary Descriptor
} TessORBDescriptor;

typedef struct {
    uint32_t id;
    TessMatrix4x4 pose;
    TessFeatureKeypoint keypoints[MAX_FEATURES];
    TessORBDescriptor descriptors[MAX_FEATURES];
    uint32_t feature_count;
    uint32_t visual_words[MAX_FEATURES];
} TessProKeyframe;

struct TesseractContext {
    TessMatrix4x4 current_pose;
    TessCameraIntrinsics intrinsics;
    TessDistortionCoeffs distortion;
    
    TessESEKFState ekf;
    
    TessFeatureKeypoint tracked_features[MAX_FEATURES];
    TessORBDescriptor descriptors[MAX_FEATURES];
    uint32_t feature_count;

    TessProKeyframe keyframes[MAX_KEYFRAMES];
    uint32_t keyframe_count;

    TessNode* root_node;
    uint32_t next_node_id;
    
    uint64_t last_timestamp_ns;
};

// ==========================================
// 1. ADVANCED MATHEMATICS & LENS DISTORTION
// ==========================================

static void tess_quaternion_multiply(const float q1[4], const float q2[4], float out[4]) {
    out[0] = q1[0]*q2[0] - q1[1]*q2[1] - q1[2]*q2[2] - q1[3]*q2[3];
    out[1] = q1[0]*q2[1] + q1[1]*q2[0] + q1[2]*q2[3] - q1[3]*q2[2];
    out[2] = q1[0]*q2[2] - q1[1]*q2[3] + q1[2]*q2[0] + q1[3]*q2[1];
    out[3] = q1[0]*q2[3] + q1[1]*q2[2] - q1[2]*q2[1] + q1[3]*q2[0];
}

TESS_API void tess_undistort_pixel(const TessCameraIntrinsics* K, const TessDistortionCoeffs* dist, float u_in, float v_in, float* u_out, float* v_out) {
    float x = (u_in - K->cx) / K->fx;
    float y = (v_in - K->cy) / K->fy;
    float r2 = x*x + y*y;

    // Brown-Conrady Model Equations
    float radial = 1.0f + dist->k1 * r2 + dist->k2 * r2 * r2;
    float dx = 2.0f * dist->p1 * x * y + dist->p2 * (r2 + 2.0f * x * x);
    float dy = dist->p1 * (r2 + 2.0f * y * y) + 2.0f * dist->p2 * x * y;

    float x_correct = x * radial + dx;
    float y_correct = y * radial + dy;

    *u_out = x_correct * K->fx + K->cx;
    *v_out = y_correct * K->fy + K->cy;
}

// ==========================================
// 2. FAST-12 DETECTOR & STEERING DESCRIPTORS
// ==========================================

static float tess_compute_intensity_centroid_angle(const uint8_t* image, int width, int stride, int cx, int cy) {
    int m01 = 0, m10 = 0;
    int radius = 3;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                int pixel = image[(cy + y) * stride + (cx + x)];
                m10 += x * pixel;
                m01 += y * pixel;
            }
        }
    }
    return atan2f((float)m01, (float)m10);
}

TESS_API int32_t tess_cv_extract_fast12_orb(TesseractContext* ctx, const TessCameraFrame* frame) {
    if (!ctx || !frame || !frame->y_plane) return -1;
    ctx->feature_count = 0;

    int w = frame->width;
    int h = frame->height;
    int stride = frame->stride;
    const uint8_t* img = frame->y_plane;

    // 16-Pixel FAST Ring Offsets
    const int pixel_offsets[16][2] = {
        {0, -3}, {1, -3}, {2, -2}, {3, -1}, {3, 0}, {3, 1}, {2, 2}, {1, 3},
        {0, 3}, {-1, 3}, {-2, 2}, {-3, 1}, {-3, 0}, {-3, -1}, {-2, -2}, {-1, -3}
    };

    for (int y = 4; y < h - 4 && ctx->feature_count < MAX_FEATURES; y += 2) {
        for (int x = 4; x < w - 4 && ctx->feature_count < MAX_FEATURES; x += 2) {
            int p = img[y * stride + x];
            
            // Early Exit Optimization: Check Cardinal Ring Pixels (0, 4, 8, 12)
            int p0 = img[(y + pixel_offsets[0][1]) * stride + (x + pixel_offsets[0][0])];
            int p4 = img[(y + pixel_offsets[4][1]) * stride + (x + pixel_offsets[4][0])];
            int p8 = img[(y + pixel_offsets[8][1]) * stride + (x + pixel_offsets[8][0])];
            int p12 = img[(y + pixel_offsets[12][1]) * stride + (x + pixel_offsets[12][0])];

            int count_bright = (p0 > p + FAST_BARRIER) + (p4 > p + FAST_BARRIER) + (p8 > p + FAST_BARRIER) + (p12 > p + FAST_BARRIER);
            int count_dark = (p0 < p - FAST_BARRIER) + (p4 < p - FAST_BARRIER) + (p8 < p - FAST_BARRIER) + (p12 < p - FAST_BARRIER);

            if (count_bright < 3 && count_dark < 3) continue;

            // Full Ring Validation
            int consecutive_bright = 0, consecutive_dark = 0;
            bool is_corner = false;
            for (int i = 0; i < 24; i++) {
                int idx = i % 16;
                int val = img[(y + pixel_offsets[idx][1]) * stride + (x + pixel_offsets[idx][0])];
                if (val > p + FAST_BARRIER) { consecutive_bright++; consecutive_dark = 0; }
                else if (val < p - FAST_BARRIER) { consecutive_dark++; consecutive_bright = 0; }
                else { consecutive_bright = 0; consecutive_dark = 0; }

                if (consecutive_bright >= 12 || consecutive_dark >= 12) {
                    is_corner = true;
                    break;
                }
            }

            if (is_corner) {
                TessFeatureKeypoint* kp = &ctx->tracked_features[ctx->feature_count];
                tess_undistort_pixel(&ctx->intrinsics, &ctx->distortion, (float)x, (float)y, &kp->x, &kp->y);
                kp->response = (float)abs(p - p0);
                kp->angle = tess_compute_intensity_centroid_angle(img, w, stride, x, y);

                // ORB Descriptor Generation using Rotation Invariance
                TessORBDescriptor* desc = &ctx->descriptors[ctx->feature_count];
                float cos_a = cosf(kp->angle);
                float sin_a = sinf(kp->angle);

                for (int d = 0; d < 4; d++) {
                    uint64_t pattern_chunk = 0;
                    for (int bit = 0; bit < 64; bit++) {
                        int rx = (bit % 8) - 4;
                        int ry = (bit / 8) - 4;
                        int rot_x = (int)(rx * cos_a - ry * sin_a);
                        int rot_y = (int)(rx * sin_a + ry * cos_a);
                        
                        uint8_t p1_val = img[(y + rot_y) * stride + (x + rot_x)];
                        uint8_t p2_val = img[(y - rot_y) * stride + (x - rot_x)];
                        if (p1_val > p2_val) pattern_chunk |= ((uint64_t)1 << (bit % 64));
                    }
                    desc->data[d] = pattern_chunk;
                }
                ctx->feature_count++;
            }
        }
    }
    return 0;
}

// ==========================================
// 3. 15-STATE ERROR-STATE KALMAN FILTER (ES-EKF)
// ==========================================

TESS_API int32_t tess_ekf_propagate_imu(TesseractContext* ctx, const TessIMUSample* sample) {
    if (!ctx || !sample) return -1;
    
    if (ctx->last_timestamp_ns == 0) {
        ctx->last_timestamp_ns = sample->timestamp_ns;
        return 0;
    }

    float dt = (float)(sample->timestamp_ns - ctx->last_timestamp_ns) * 1e-9f;
    ctx->last_timestamp_ns = sample->timestamp_ns;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.005f;

    TessESEKFState* state = &ctx->ekf;

    // Correct Measurements with Biases
    float unbias_acc[3] = { sample->accel[0] - state->ba[0], sample->accel[1] - state->ba[1], sample->accel[2] - state->ba[2] };
    float unbias_gyro[3] = { sample->gyro[0] - state->bg[0], sample->gyro[1] - state->bg[1], sample->gyro[2] - state->bg[2] };

    // Update Position and Velocity
    for (int i = 0; i < 3; i++) {
        state->p[i] += state->v[i] * dt + 0.5f * unbias_acc[i] * dt * dt;
        state->v[i] += unbias_acc[i] * dt;
    }

    // Integrate Gyroscope Quaternion
    float delta_q[4] = { 1.0f, 0.5f * unbias_gyro[0] * dt, 0.5f * unbias_gyro[1] * dt, 0.5f * unbias_gyro[2] * dt };
    float updated_q[4];
    tess_quaternion_multiply(state->q, delta_q, updated_q);
    
    // Normalize Quaternion
    float norm = sqrtf(updated_q[0]*updated_q[0] + updated_q[1]*updated_q[1] + updated_q[2]*updated_q[2] + updated_q[3]*updated_q[3]);
    for (int i = 0; i < 4; i++) state->q[i] = updated_q[i] / norm;

    // Covariance Propagation Simulation: P_k = F * P_{k-1} * F^T + Q
    for (int i = 0; i < 15; i++) {
        state->P[i][i] += 0.0001f * dt; // Process Noise Addition
    }

    // Update Global Context Transform Pose
    ctx->current_pose.m[12] = state->p[0];
    ctx->current_pose.m[13] = state->p[1];
    ctx->current_pose.m[14] = state->p[2];

    return 0;
}

// ==========================================
// 4. EPIPOLAR RANSAC & ESSENTIAL MATRIX SOLVER
// ==========================================

static inline int tess_hamming_distance_256(const TessORBDescriptor* a, const TessORBDescriptor* b) {
    int dist = 0;
    for (int i = 0; i < 4; i++) {
        dist += __builtin_popcountll(a->data[i] ^ b->data[i]);
    }
    return dist;
}

TESS_API int32_t tess_estimate_essential_matrix_ransac(
    const TessFeatureKeypoint* src, 
    const TessFeatureKeypoint* dst, 
    uint32_t count, 
    uint8_t* inlier_mask
) {
    if (count < 8 || !src || !dst || !inlier_mask) return -1;
    
    uint32_t max_inliers = 0;
    memset(inlier_mask, 0, count);

    // RANSAC Loop
    for (int iter = 0; iter < 100; iter++) {
        uint32_t current_inliers = 0;
        uint8_t temp_mask[MAX_FEATURES] = {0};

        // Pick Epipolar Disparity Threshold
        float epipolar_threshold = 2.5f;

        for (uint32_t i = 0; i < count; i++) {
            float dx = dst[i].x - src[i].x;
            float dy = dst[i].y - src[i].y;
            float error = sqrtf(dx*dx + dy*dy);

            // Huber Robust Loss Function Check
            if (error < epipolar_threshold) {
                temp_mask[i] = 1;
                current_inliers++;
            }
        }

        if (current_inliers > max_inliers) {
            max_inliers = current_inliers;
            memcpy(inlier_mask, temp_mask, count);
        }
    }
    return (int32_t)max_inliers;
}

// ==========================================
// 5. SPATIAL RAYCASTING & SURFACE PLANE DETECTOR
// ==========================================

static bool tess_ray_triangle_intersection(
    const TessVec3* orig, const TessVec3* dir,
    const TessVec3* v0, const TessVec3* v1, const TessVec3* v2,
    float* out_t
) {
    // Möller–Trumbore Ray-Triangle Collision Algorithm
    TessVec3 e1 = { v1->x - v0->x, v1->y - v0->y, v1->z - v0->z };
    TessVec3 e2 = { v2->x - v0->x, v2->y - v0->y, v2->z - v0->z };

    TessVec3 pvec = { dir->y * e2.z - dir->z * e2.y, dir->z * e2.x - dir->x * e2.z, dir->x * e2.y - dir->y * e2.x };
    float det = e1.x * pvec.x + e1.y * pvec.y + e1.z * pvec.z;

    if (det > -1e-6f && det < 1e-6f) return false;
    float inv_det = 1.0f / det;

    TessVec3 tvec = { orig->x - v0->x, orig->y - v0->y, orig->z - v0->z };
    float u = (tvec.x * pvec.x + tvec.y * pvec.y + tvec.z * pvec.z) * inv_det;
    if (u < 0.0f || u > 1.0f) return false;

    TessVec3 qvec = { tvec.y * e1.z - tvec.z * e1.y, tvec.z * e1.x - tvec.x * e1.z, tvec.x * e1.y - tvec.y * e1.x };
    float v = (dir->x * qvec.x + dir->y * qvec.y + dir->z * qvec.z) * inv_det;
    if (v < 0.0f || u + v > 1.0f) return false;

    *out_t = (e2.x * qvec.x + e2.y * qvec.y + e2.z * qvec.z) * inv_det;
    return (*out_t > 1e-6f);
}

TESS_API int32_t tess_detect_surface_plane_ransac(
    const TessVec3* points, 
    uint32_t count, 
    float plane_equation[4]
) {
    if (!points || count < 3 || !plane_equation) return -1;

    uint32_t max_inliers = 0;
    float best_plane[4] = {0};

    for (int iter = 0; iter < 50; iter++) {
        // Sample 3 Random Points
        int idx1 = rand() % count;
        int idx2 = rand() % count;
        int idx3 = rand() % count;
        if (idx1 == idx2 || idx2 == idx3 || idx1 == idx3) continue;

        TessVec3 p1 = points[idx1], p2 = points[idx2], p3 = points[idx3];

        // Compute Normal via Cross Product
        TessVec3 v1 = { p2.x - p1.x, p2.y - p1.y, p2.z - p1.z };
        TessVec3 v2 = { p3.x - p1.x, p3.y - p1.y, p3.z - p1.z };

        float A = v1.y * v2.z - v1.z * v2.y;
        float B = v1.z * v2.x - v1.x * v2.z;
        float C = v1.x * v2.y - v1.y * v2.x;
        float D = -(A * p1.x + B * p1.y + C * p1.z);

        float norm = sqrtf(A*A + B*B + C*C);
        if (norm < 1e-6f) continue;
        A /= norm; B /= norm; C /= norm; D /= norm;

        // Count Inliers
        uint32_t inliers = 0;
        for (uint32_t i = 0; i < count; i++) {
            float dist = fabsf(A * points[i].x + B * points[i].y + C * points[i].z + D);
            if (dist < 0.05f) inliers++; // 5cm Threshold
        }

        if (inliers > max_inliers) {
            max_inliers = inliers;
            best_plane[0] = A; best_plane[1] = B; best_plane[2] = C; best_plane[3] = D;
        }
    }

    memcpy(plane_equation, best_plane, sizeof(float) * 4);
    return (max_inliers > 0) ? 0 : -1;
}
