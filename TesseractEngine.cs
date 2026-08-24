using System;
using System.Runtime.InteropServices;

public class TesseractEngine : IDisposable
{
    private const string LIB_NAME = "tesseract";

    [StructLayout(LayoutKind.Sequential)]
    public struct TessMatrix4x4
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public float[] m;
    }

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr tess_create();

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern void tess_destroy(IntPtr ctx);

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern int tess_process_frame(IntPtr ctx, float deltaTime);

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern int tess_transform_vector(IntPtr ctx, float[] inVec3, float[] outVec3);

    private IntPtr _ctx;

    public TesseractEngine()
    {
        _ctx = tess_create();
        if (_ctx == IntPtr.Zero)
            throw new InvalidOperationException("Failed to allocate Tesseract context.");
    }

    public void Update(float deltaTime)
    {
        if (_ctx == IntPtr.Zero) return;
        int status = tess_process_frame(_ctx, deltaTime);
        if (status != 0) throw new Exception($"Tesseract frame error: {status}");
    }

    public float[] TransformVector(float[] inVec3)
    {
        if (inVec3.Length != 3) throw new ArgumentException("Input must be 3 floats.");
        float[] outVec3 = new float[3];
        int status = tess_transform_vector(_ctx, inVec3, outVec3);
        if (status != 0) throw new Exception($"Vector transformation error: {status}");
        return outVec3;
    }

    public void Dispose()
    {
        if (_ctx != IntPtr.Zero)
        {
            tess_destroy(_ctx);
            _ctx = IntPtr.Zero;
        }
    }

    ~TesseractEngine() => Dispose();
}
