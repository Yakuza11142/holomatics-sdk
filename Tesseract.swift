import Foundation

public class TesseractEngine {
    private var ctx: OpaquePointer?

    public init() {
        self.ctx = tess_create()
    }

    public func update(deltaTime: Float) -> Bool {
        guard let context = ctx else { return false }
        let result = tess_process_frame(context, deltaTime)
        return result == 0
    }

    deinit {
        if let context = ctx {
            tess_destroy(context)
        }
    }
}
