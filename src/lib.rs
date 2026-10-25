#![cfg_attr(not(any(test, kani)), no_std)]

#[cfg(not(any(test, kani)))]
#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn tesseract_engine_version() -> i32 {
    100
}
