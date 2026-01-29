package net.Chivent.pmxSteveMod.jni;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;

public final class PmxNative {
    private static final String LIB_NAME = "PmxModJNI";

    static {
        loadNative();
    }

    private PmxNative() {}

    private static void loadNative() {
        String libFile = System.mapLibraryName(LIB_NAME);
        String resourcePath = "/assets/pmx_steve_mod/native/" + libFile;
        try (InputStream in = PmxNative.class.getResourceAsStream(resourcePath)) {
            if (in == null) {
                System.loadLibrary(LIB_NAME);
                return;
            }
            Path outDir = Paths.get(System.getProperty("java.io.tmpdir"), "pmx_steve_mod", "native");
            Files.createDirectories(outDir);
            Path outFile = outDir.resolve(libFile);
            Files.copy(in, outFile, StandardCopyOption.REPLACE_EXISTING);
            System.load(outFile.toString());
        } catch (IOException e) {
            throw new RuntimeException("Failed to load native library: " + libFile, e);
        }
    }

    public static native long nativeCreate();
    public static native void nativeDestroy(long h);

    public static native boolean nativeLoadPmx(long h, String pmxPath, String dataDir);
    public static native boolean nativeAddVmd(long h, String vmdPath);
    public static native boolean nativeStartVmdBlend(long h, String vmdPath, float blendSeconds);
    public static native boolean nativeStartDefaultPoseBlend(long h, float blendSeconds);
    public static native float nativeGetMotionMaxFrame(long h);
    public static native boolean nativeLoadCameraVmd(long h, String vmdPath);
    public static native void nativeClearCamera(long h);
    public static native boolean nativeGetCameraState(long h, float frame, java.nio.ByteBuffer dst8f);
    public static native boolean nativePlayMusicLoop(long h, String musicPath, boolean loop);
    public static native double nativeGetMusicLengthSec(long h);
    public static native void nativeSetMusicVolume(long h, float volume);
    public static native void nativeStopMusic(long h);
    public static native boolean nativeGetMusicTimes(long h, java.nio.ByteBuffer dst2f);
    public static native void nativeSyncPhysics(long h, float t);
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
    public static native int nativeGetSubmeshBeginIndex(long h, int s);
    public static native int nativeGetSubmeshIndexCount(long h, int s);
    public static native int nativeGetSubmeshMaterialId(long h, int s);

    public static native int nativeGetMaterialCount(long h);

    public static native int nativeGetMaterialDiffuseRGBA(long h, int m);
    public static native float nativeGetMaterialAlpha(long h, int m);

    public static native void nativeGetMaterialAmbient(long h, int m, java.nio.ByteBuffer dst3f);
    public static native void nativeGetMaterialSpecular(long h, int m, java.nio.ByteBuffer dst3f);
    public static native float nativeGetMaterialSpecularPower(long h, int m);
    public static native boolean nativeGetMaterialBothFaceByMaterial(long h, int m);

    public static native String nativeGetMaterialTexturePath(long h, int m);
    public static native String nativeGetMaterialSphereTexturePath(long h, int m);
    public static native int nativeGetMaterialSphereMode(long h, int m); // 0 none, 1 mul, 2 add
    public static native String nativeGetMaterialToonTexturePath(long h, int m);

    public static native void nativeGetMaterialTexMulFactor(long h, int m, java.nio.ByteBuffer dst4f);
    public static native void nativeGetMaterialTexAddFactor(long h, int m, java.nio.ByteBuffer dst4f);
    public static native void nativeGetMaterialSphereMulFactor(long h, int m, java.nio.ByteBuffer dst4f);
    public static native void nativeGetMaterialSphereAddFactor(long h, int m, java.nio.ByteBuffer dst4f);
    public static native void nativeGetMaterialToonMulFactor(long h, int m, java.nio.ByteBuffer dst4f);
    public static native void nativeGetMaterialToonAddFactor(long h, int m, java.nio.ByteBuffer dst4f);

    public static native int nativeGetMaterialEdgeFlag(long h, int m);
    public static native float nativeGetMaterialEdgeSize(long h, int m);
    public static native void nativeGetMaterialEdgeColor(long h, int m, java.nio.ByteBuffer dst4f);
    public static native boolean nativeGetMaterialGroundShadow(long h, int m);
    public static native boolean nativeGetMaterialShadowCaster(long h, int m);
    public static native boolean nativeGetMaterialShadowReceiver(long h, int m);
}
