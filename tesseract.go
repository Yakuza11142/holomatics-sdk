package tesseract

/*
#cgo CFLAGS: -I.
#cgo LDFLAGS: -L. -ltesseract
#include "tesseract_engine.h"
*/
import "C"
import (
	"errors"
	"unsafe"
)

type Engine struct {
	ctx *C.TesseractContext
}

func NewEngine() (*Engine, error) {
	ctx := C.tess_create()
	if ctx == nil {
		return nil, errors.New("failed to initialize native tesseract context")
	}
	return &Engine{ctx: ctx}, nil
}

func (e *Engine) Close() {
	if e.ctx != nil {
		C.tess_destroy(e.ctx)
		e.ctx = nil
	}
}

func (e *Engine) ProcessFrame(deltaTime float32) error {
	res := C.tess_process_frame(e.ctx, C.float(deltaTime))
	if res != 0 {
		return errors.New("tesseract engine processing error")
	}
	return nil
}

func (e *Engine) TransformVector(inVec [3]float32) ([3]float32, error) {
	var outVec [3]float32
	res := C.tess_transform_vector(
		e.ctx,
		(*C.float)(unsafe.Pointer(&inVec[0])),
		(*C.float)(unsafe.Pointer(&outVec[0])),
	)
	if res != 0 {
		return outVec, errors.New("failed to transform vector in native memory")
	}
	return outVec, nil
}
