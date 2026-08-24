import ctypes
import os
import platform

class TessMatrix4x4(ctypes.Structure):
    _fields_ = [("m", ctypes.c_float * 16)]

class TesseractEngine:
    def __init__(self, lib_path=None):
        if lib_path is None:
            system = platform.system()
            if system == "Darwin":
                lib_path = "./libtesseract.dylib"
            elif system == "Windows":
                lib_path = "./tesseract.dll"
            else:
                lib_path = "./libtesseract.so"

        self._lib = ctypes.CDLL(os.path.abspath(lib_path))

        # Setup FFI Function Signatures
        self._lib.tess_create.restype = ctypes.c_void_p
        self._lib.tess_destroy.argtypes = [ctypes.c_void_p]
        
        self._lib.tess_process_frame.argtypes = [ctypes.c_void_p, ctypes.c_float]
        self._lib.tess_process_frame.restype = ctypes.c_int32

        self._lib.tess_transform_vector.argtypes = [
            ctypes.c_void_p, 
            ctypes.POINTER(ctypes.c_float * 3), 
            ctypes.POINTER(ctypes.c_float * 3)
        ]
        self._lib.tess_transform_vector.restype = ctypes.c_int32

        # Create Native Context
        self._ctx = self._lib.tess_create()
        if not self._ctx:
            raise RuntimeError("Failed to initialize Tesseract Engine context")

    def update(self, delta_time: float):
        res = self._lib.tess_process_frame(self._ctx, ctypes.c_float(delta_time))
        if res != 0:
            raise RuntimeError(f"Tesseract Error Code: {res}")

    def transform_vector(self, vec3_in: list[float]) -> list[float]:
        in_arr = (ctypes.c_float * 3)(*vec3_in)
        out_arr = (ctypes.c_float * 3)()
        res = self._lib.tess_transform_vector(self._ctx, ctypes.byref(in_arr), ctypes.byref(out_arr))
        if res != 0:
            raise RuntimeError(f"Vector transformation failed with code: {res}")
        return list(out_arr)

    def close(self):
        if self._ctx:
            self._lib.tess_destroy(self._ctx)
            self._ctx = None

    def __del__(self):
        self.close()
