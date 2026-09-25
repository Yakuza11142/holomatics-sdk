import 'dart:ffi';
import 'dart:io';

// Native function signature declarations for Dart FFI mapping
typedef NativeCreateFunc = Pointer<Void> Function();
typedef CreateFunc = Pointer<Void> Function();

typedef NativeDestroyFunc = Void Function(Pointer<Void>);
typedef DestroyFunc = void Function(Pointer<Void>);

typedef NativeProcessFunc = Int32 Function(Pointer<Void>, Float);
typedef ProcessFunc = int Function(Pointer<Void>, double);

class TessFlutter {
  // 1. Unified Binary Target Lookup Alignment
  static final DynamicLibrary _nativeLib = Platform.isAndroid
      ? DynamicLibrary.open('libTessSDK.so')
      : DynamicLibrary.process();

  // 2. Safe Bare-Metal C FFI Function Mappings
  static final CreateFunc _tessCreate = _nativeLib
      .lookup<NativeFunction<NativeCreateFunc>>('tess_create')
      .asFunction();

  static final DestroyFunc _tessDestroy = _nativeLib
      .lookup<NativeFunction<NativeDestroyFunc>>('tess_destroy')
      .asFunction();

  static final ProcessFunc _tessProcessFrame = _nativeLib
      .lookup<NativeFunction<NativeProcessFunc>>('tess_process_frame')
      .asFunction();

  // Instance tracking references
  Pointer<Void>? _context;

  /// Allocates and boots up a low-level native runtime context pipeline.
  void initEngine() {
    if (_context != null) return;
    
    final nativePtr = _tessCreate();
    if (nativePtr == nullptr) {
      throw Exception('TessFlutter: Failed to allocate low-level native runtime context pipeline.');
    }
    _context = nativePtr;
  }

  /// Updates the spatial core matrix state inside your Flutter render loops.
  void processFrame(double deltaTime) {
    final ctx = _context;
    if (ctx == null) return;

    // Guard against negative or erratic delta timing anomalies
    if (deltaTime <= 0.0 || deltaTime >= 1.0) return;

    final resultCode = _tessProcessFrame(ctx, deltaTime);
    if (resultCode != 0) {
      print('⚠️ TessFlutter: Native pipeline frame processing error: $resultCode');
    }
  }

  /// Safely deallocates the engine pointer memory space when the component disposes.
  void dispose() {
    final ctx = _context;
    if (ctx != null) {
      _tessDestroy(ctx);
      _context = null;
    }
  }
}
