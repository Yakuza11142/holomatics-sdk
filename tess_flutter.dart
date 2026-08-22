import 'dart:ffi';
import 'dart:io';

typedef NativeInitFunc = Void Function();
typedef InitFunc = void Function();

class TessFlutter {
  static final DynamicLibrary _nativeLib = Platform.isAndroid
      ? DynamicLibrary.open('libtessnative.so')
      : DynamicLibrary.process();

  static final InitFunc initEngine = _nativeLib
      .lookup<NativeFunction<NativeInitFunc>>('Java_com_tesseract_ui_TessEngine_nativeInit')
      .asFunction();

  static void initialize() {
    initEngine();
  }
}
