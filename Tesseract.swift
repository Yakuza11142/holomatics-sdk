import Foundation
import os.lock

// Foreign C/Rust opaque handle marked Sendable for Swift 6 strict concurrency checks
private struct TesseractContext: @unchecked Sendable {
    let pointer: OpaquePointer
}

/// A thread-safe, high-performance Swift wrapper interface for the native Tesseract spatial core.
/// Built with low-overhead unfair lock synchronization to eliminate race conditions across async sensor threads.
public final class TesseractEngine: Sendable {

    // Low-overhead stack atomic lock (zero allocation cost compared to NSLock)
    private let state: OSAllocatedUnfairLock<State>

    private struct State {
        var context: TesseractContext?
    }

    public init() {
        guard let nativePtr = tess_create() else {
            fatalError("Tesseract Engine: Failed to instantiate low-level native runtime context pipeline.")
        }
        self.state = OSAllocatedUnfairLock(State(context: TesseractContext(pointer: nativePtr)))
    }

    /// Processes an engine frame tick.
    /// - Parameter deltaTime: Elapsed frame duration in seconds.
    /// - Returns: `true` if the spatial matrix update processed cleanly without hardware errors.
    @discardableResult
    public func update(deltaTime: Float) -> Bool {
        // Guard against negative, zero, or erratic delta timing injection anomalies
        guard deltaTime > 0.0 && deltaTime < 1.0 else { return false }

        return state.withLock { currentState in
            guard let context = currentState.context else { return false }

            // Execute native bare-metal C/Rust library execution binding loops
            let result = tess_process_frame(context.pointer, deltaTime)
            return result == 0
        }
    }

    deinit {
        // Extract pointer and invalidate engine context atomically
        let contextToDestroy = state.withLock { currentState -> TesseractContext? in
            let ctx = currentState.context
            currentState.context = nil
            return ctx
        }

        if let context = contextToDestroy {
            // Safely deallocate the engine pointer memory space
            tess_destroy(context.pointer)
        }
    }
}
