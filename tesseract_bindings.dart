import 'dart:ffi';
import 'dart:io';
import 'package:ffi/ffi.dart';

// FFI Signatures
typedef NativeCreate = Pointer<Void> Function();
typedef DartCreate = Pointer<Void> Function();

typedef NativeDestroy = Void Function(Pointer<Void>);
typedef DartDestroy = void Function(Pointer<Void>);

typedef NativeProcess = Int32 Function(Pointer<Void>, Float);
typedef DartProcess = int Function(Pointer<Void>, double);

class TesseractEngine {
  late DynamicLibrary _lib;
  late DartCreate _create;
  late DartDestroy _destroy;
  late DartProcess _process;

  Pointer<Void>? _ctx;

  TesseractEngine() {
    // Dynamic library loader across desktop & mobile platforms
    if (Platform.isAndroid) {
      _lib = DynamicLibrary.open("libtesseract.so");
    } else if (Platform.isIOS || Platform.isMacOS) {
      _lib = DynamicLibrary.process();
    } else if (Platform.isLinux) {
      _lib = DynamicLibrary.open("libtesseract.so");
    } else if (Platform.isWindows) {
      _lib = DynamicLibrary.open("tesseract.dll");
    } else {
      throw UnsupportedError("Unsupported platform");
    }

    _create = _lib.lookupFunction<NativeCreate, DartCreate>('tess_create');
    _destroy = _lib.lookupFunction<NativeDestroy, DartDestroy>('tess_destroy');
    _process = _lib.lookupFunction<NativeProcess, DartProcess>('tess_process_frame');

    _ctx = _create();
  }

  void update(double deltaTime) {
    if (_ctx != null) {
      int result = _process(_ctx!, deltaTime);
      if (result != 0) {
        throw Exception("Tesseract processing error: $result");
      }
    }
  }

  void dispose() {
    if (_ctx != null) {
      _destroy(_ctx!);
      _ctx = null;
    }
  }
}
