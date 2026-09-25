// tesseract.h
#ifndef TESSERACT_H
#define TESSERACT_H

#ifdef __cplusplus
extern "C" {
#endif

// Core Lifecycle
void tesseract_start(void);
void tesseract_shutdown(void);

// Vector & Matrix Operations
void tesseract_set_position(float x, float y, float z);
void tesseract_render_frame(void);

#ifdef __cplusplus
}
#endif

#endif // TESSERACT_H
