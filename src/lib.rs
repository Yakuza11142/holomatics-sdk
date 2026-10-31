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

// --- Inline Unit Tests for Miri, TSAN, and MSan ---
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_engine_version() {
        assert_eq!(tesseract_engine_version(), 100);
    }
}

// --- Kani Formal Proof Harness ---
#[cfg(kani)]
#[kani::proof]
fn verify_engine_version_bounds() {
    let version = tesseract_engine_version();
    // Formally verify that the version code is always within expected bounds
    assert!(version > 0);
    assert!(version <= 1000);
}
