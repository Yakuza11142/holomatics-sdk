import Foundation

/// A thread-safe, high-performance wrapper interface for the native Tesseract spatial core.
/// Isolated using a dedicated global actor or explicit threading locks to prevent concurrent race conditions.
public final class TesseractEngine: Sendable {
    
    // Use a lock to ensure absolute multi-threaded synchronization across async sensor tasks
    private let lock = NSLock()
    private var ctx: OpaquePointer?

    public init() {
        self.ctx = tess_create()
        guard self.ctx != nil else {
            fatalError("Tesseract Engine: Failed to instantiate low-level native runtime context pipeline.")
        }
    }

    /// Processes an engine frame tick. 
    /// - Parameter deltaTime: Elapsed frame duration in seconds.
    /// - Returns: True if the spatial matrix update processed cleanly without hardware errors.
    @discardableResult
    public func update(deltaTime: Float) -> Bool {
        // Guard against negative, zero, or erratic delta timing injection anomalies
        guard deltaTime > 0.0 && deltaTime < 1.0 else { return false }
        
        lock.lock()
        defer { lock.unlock() }
        
        guard let context = ctx else { return false }
        
        // Execute native bare-metal C/Rust library execution binding loops
        let result = tess_process_frame(context, deltaTime)
        return result == 0
    }

    deinit {
        // Enforce cleanup closure isolation
        let contextToDestroy = self.ctx
        self.ctx = nil
        
        if let context = contextToDestroy {
            // Safely deallocate the engine pointer memory space
            tess_destroy(context)
        }
    }
}
