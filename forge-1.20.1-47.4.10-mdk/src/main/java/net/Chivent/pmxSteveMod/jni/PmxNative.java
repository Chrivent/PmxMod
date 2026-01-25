package net.Chivent.pmxSteveMod.jni;

public final class PmxNative {
    static {
        System.loadLibrary("PmxModJNI");
    }

    public static native long nativeCreate();
    public static native void nativeDestroy(long h);

    public static native boolean nativeLoadPmx(long h, String pmxPath, String dataDir);
    public static native boolean nativeAddVmd(long h, String vmdPath);

    public static native void nativeUpdate(long h, float frame, float physicsElapsed);

    public static native String nativeGetModelName(long h);
    public static native String nativeGetEnglishModelName(long h);
    public static native String nativeGetComment(long h);
    public static native String nativeGetEnglishComment(long h);

    public static native int nativeGetVertexCount(long h);
    public static native int nativeGetIndexCount(long h);
    public static native int nativeGetIndexElementSize(long h);

    public static native void nativeCopyIndices(long h, java.nio.ByteBuffer dst);
    public static native void nativeCopyPositions(long h, java.nio.FloatBuffer dst);
    public static native void nativeCopyNormals(long h, java.nio.FloatBuffer dst);
    public static native void nativeCopyUVs(long h, java.nio.FloatBuffer dst);
}
