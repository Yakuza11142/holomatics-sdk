#![cfg_attr(not(test), no_std)]

#[cfg(not(test))]
#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn tesseract_engine_version() -> i32 {
    100
}
