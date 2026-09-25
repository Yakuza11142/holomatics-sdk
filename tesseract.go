package tesseract

/*
#cgo CFLAGS: -I.
#cgo LDFLAGS: -L. -lTessSDK
#include "tesseract_engine.h"
*/
import "C"
import (
	"errors"
	"sync"
	"unsafe"
)

// Engine represents a thread-safe Go wrapper for the native Tesseract spatial runtime.
type Engine struct {
	mu  sync.Mutex
	ctx *C.TesseractContext
}

// NewEngine allocates and initializes the underlying native Tesseract context.
func NewEngine() (*Engine, error) {
	ctx := C.tess_create()
	if ctx == nil {
		return nil, errors.New("failed to initialize native tesseract context")
	}
	return &Engine{ctx: ctx}, nil
}

// Close cleanly tears down native allocations and invalidates the context pointer.
func (e *Engine) Close() {
	e.mu.Lock()
	defer e.mu.Unlock()

	if e.ctx != nil {
		C.tess_destroy(e.ctx)
		e.ctx = nil
	}
}

// ProcessFrame signals the native rasterization engine to tick the spatial core matrix state.
func (e *Engine) ProcessFrame(deltaTime float32) error {
	e.mu.Lock()
	defer e.mu.Unlock()

	if e.ctx == nil {
		return errors.New("engine context is closed or uninitialized")
	}

	res := C.tess_process_frame(e.ctx, C.float(deltaTime))
	if res != 0 {
		return errors.New("tesseract engine processing error")
	}
	return nil
}

// TransformVector performs 3D spatial coordinate conversion inside bare-metal memory spaces.
// Utilizes localized pointer anchoring to maintain memory location stability during execution.
func (e *Engine) TransformVector(inVec [3]float32) ([3]float32, error) {
	var outVec [3]float32

	e.mu.Lock()
	defer e.mu.Unlock()

	if e.ctx == nil {
		return outVec, errors.New("engine context is closed or uninitialized")
	}

	// Safely anchor local array references during the transition across the FFI gate
	inPtr := (*C.float)(unsafe.Pointer(&inVec[0]))
	outPtr := (*C.float)(unsafe.Pointer(&outVec[0]))

	res := C.tess_transform_vector(e.ctx, inPtr, outPtr)
	if res != 0 {
		return outVec, errors.New("failed to transform vector in native memory")
	}
	return outVec, nil
}
