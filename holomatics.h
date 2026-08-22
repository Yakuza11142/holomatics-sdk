// holomatics.h
#ifndef HOLOMATICS_H
#define HOLOMATICS_H

#ifdef __cplusplus
extern "C" {
#endif

// Core Lifecycle
void holomatics_start(void);
void holomatics_shutdown(void);

// Vector & Matrix Operations
void holomatics_set_position(float x, float y, float z);
void holomatics_render_frame(void);

#ifdef __cplusplus
}
#endif

#endif
