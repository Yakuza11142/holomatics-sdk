#![no_std]

#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    loop {}
}

/// Example FFI hook exported to TessBridge.c / libTessSDK.so
#[no_mangle]
pub extern "C" fn tesseract_engine_version() -> i32 {
    100
}
