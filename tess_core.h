#ifndef TESS_CORE_H
#define TESS_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

// Universal Core Functions
void tess_init(void);
void tess_render_frame(float deltaTime);
void tess_process_gesture(float x, float y);

#ifdef __cplusplus
}
#endif

#endif // TESS_CORE_H
