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
    public static native void nativeCopyPositions(long h, java.nio.ByteBuffer dst);
    public static native void nativeCopyNormals(long h, java.nio.ByteBuffer dst);
    public static native void nativeCopyUVs(long h, java.nio.ByteBuffer dst);

    public static native int nativeGetSubmeshCount(long h);
    public static native int nativeGetSubmeshBeginIndex(long h, int m);
    public static native int nativeGetSubmeshVertexCount(long h, int m);
    public static native String nativeGetSubmeshTexturePath(long h, int m);
    public static native float nativeGetSubmeshAlpha(long h, int m);
    public static native int nativeGetSubmeshDiffuseRGBA(long h, int m);
    public static native boolean nativeGetSubmeshBothFace(long h, int s);
}
