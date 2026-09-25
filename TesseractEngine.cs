using System;
using System.Runtime.InteropServices;

/// <summary>
/// Thread-safe, low-allocation C# P/Invoke wrapper for the native Tesseract spatial engine (<c>libTessSDK.so</c> / <c>TessSDK.dll</c> / <c>libTessSDK.dylib</c>).
/// </summary>
public sealed class TesseractEngine : IDisposable
{
    // FIX: Updated to match your unified dynamic library target binary artifact name
    private const string LIB_NAME = "TessSDK";

    // ----------------------------------------------------------------------------
    // NATIVE STRUCTS & LAYOUTS (Zero-Allocation Inline Buffers)
    // ----------------------------------------------------------------------------

    /// <summary>
    /// 4x4 Matrix representation with continuous memory layout matching C/C++ alignment.
    /// Uses value-type fixed fields to guarantee zero heap allocation.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public unsafe struct TessMatrix4x4
    {
        public fixed float m[16];
    }

    /// <summary>
    /// L1 Spherical Harmonics struct matching C vision pipeline.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public unsafe struct SHCoefficients
    {
        public fixed float r[4];
        public fixed float g[4];
        public fixed float b[4];
    }

    // ----------------------------------------------------------------------------
    // P/INVOKE NATIVE BINDINGS
    // ----------------------------------------------------------------------------

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr tess_create();

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern void tess_destroy(IntPtr ctx);

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl, EntryPoint = "tess_process_frame")]
    private static extern int tess_process_frame(IntPtr ctx, float deltaTime);

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl, EntryPoint = "tess_transform_vector")]
    private static unsafe extern int tess_transform_vector(IntPtr ctx, float* inVec3, float* outVec3);

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl, EntryPoint = "Tess_EstimateAmbientLighting")]
    private static unsafe extern SHCoefficients Tess_EstimateAmbientLighting(byte* framePtr, int width, int height);

    // ----------------------------------------------------------------------------
    // WRAPPER IMPLEMENTATION
    // ----------------------------------------------------------------------------

    private readonly object _lock = new object();
    private IntPtr _ctx;
    private bool _disposed;

    public TesseractEngine()
    {
        _ctx = tess_create();
        if (_ctx == IntPtr.Zero)
        {
            throw new InvalidOperationException("Tesseract Engine: Failed to allocate low-level native runtime context pipeline.");
        }
    }

    /// <summary>
    /// Updates the spatial core matrix state by frame delta time.
    /// </summary>
    /// <param name="deltaTime">Frame time step in seconds (must be within range 0.0 to 1.0).</param>
    public void Update(float deltaTime)
    {
        if (deltaTime <= 0.0f || deltaTime >= 1.0f) return;

        lock (_lock)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            if (_ctx == IntPtr.Zero) return;

            int status = tess_process_frame(_ctx, deltaTime);
            if (status != 0)
            {
                throw new InvalidOperationException($"Tesseract frame update failed with error code: {status}");
            }
        }
    }

    /// <summary>
    /// Performs vector transformation directly in native memory using stack allocation (Zero-GC).
    /// </summary>
    public unsafe void TransformVector(ReadOnlySpan<float> inVec3, Span<float> outVec3)
    {
        if (inVec3.Length < 3 || outVec3.Length < 3)
        {
            throw new ArgumentException("Input and Output vector spans must contain at least 3 elements.");
        }

        lock (_lock)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);

            fixed (float* pIn = inVec3)
            fixed (float* pOut = outVec3)
            {
                int status = tess_transform_vector(_ctx, pIn, pOut);
                if (status != 0)
                {
                    throw new InvalidOperationException($"Vector transformation failed with error code: {status}");
                }
            }
        }
    }

    /// <summary>
    /// Extracts Spherical Harmonics ambient lighting from a raw RGBA byte buffer directly without GC pin overhead.
    /// </summary>
    public unsafe SHCoefficients EstimateLighting(ReadOnlySpan<byte> rgbaBuffer, int width, int height)
    {
        if (rgbaBuffer.IsEmpty || width <= 0 || height <= 0)
        {
            return default;
        }

        lock (_lock)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);

            fixed (byte* pBuffer = rgbaBuffer)
            {
                return Tess_EstimateAmbientLighting(pBuffer, width, height);
            }
        }
    }

    // ----------------------------------------------------------------------------
    // DISPOSE PATTERN (RAII & Finalizer Safe)
    // ----------------------------------------------------------------------------

    private void Dispose(bool disposing)
    {
        if (!_disposed)
        {
            lock (_lock)
            {
                if (_ctx != IntPtr.Zero)
                {
                    tess_destroy(_ctx);
                    _ctx = IntPtr.Zero;
                }
            }
            _disposed = true;
        }
    }

    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    ~TesseractEngine()
    {
        Dispose(false);
    }
}
