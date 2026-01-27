package net.Chivent.pmxSteveMod.viewer;

import com.mojang.logging.LogUtils;
import net.Chivent.pmxSteveMod.jni.PmxNative;
import org.slf4j.Logger;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Path;
import java.nio.file.Paths;

public class PmxViewer {
    private static final Logger LOGGER = LogUtils.getLogger();
    private static final PmxViewer INSTANCE = new PmxViewer();
    private final PmxRenderer renderer = new PmxRenderer();
    public static PmxViewer get() { return INSTANCE; }
    public PmxRenderer renderer() { return renderer; }

    private long handle = 0L;
    private boolean ready = false;

    private float frame = 0f;
    private long lastNanos = -1;

    private ByteBuffer idxBuf;
    private ByteBuffer posBuf;
    private ByteBuffer nrmBuf;
    private ByteBuffer uvBuf;

    private Path pmxBaseDir = null;

    private SubmeshInfo[] submeshes;
    private MaterialInfo[] materials;

    private final ByteBuffer tmp3f = ByteBuffer.allocateDirect(3 * 4).order(ByteOrder.nativeOrder());
    private final ByteBuffer tmp4f = ByteBuffer.allocateDirect(4 * 4).order(ByteOrder.nativeOrder());

    private static final boolean FLIP_V = true;

    private PmxViewer() {}

    public boolean isReady() { return ready; }
    public long handle() { return handle; }

    public ByteBuffer idxBuf() { return idxBuf; }
    public ByteBuffer posBuf() { return posBuf; }
    public ByteBuffer nrmBuf() { return nrmBuf; }
    public ByteBuffer uvBuf()  { return uvBuf;  }

    public boolean flipV() { return FLIP_V; }

    public SubmeshInfo[] submeshes() { return submeshes; }

    public MaterialInfo material(int id) {
        if (materials == null || id < 0 || id >= materials.length) return null;
        return materials[id];
    }

    public Path pmxBaseDir() { return pmxBaseDir; }

    public void init() {
        try {
            handle = PmxNative.nativeCreate();

            String dataDir = "C:/Users/Ha Yechan/Desktop/PmxMod/resource/mmd";
            String pmxPath = "D:/예찬/MMD/model/Booth/Chrivent Elf/Chrivent Elf.pmx";
            pmxBaseDir = Paths.get(pmxPath).getParent();

            boolean ok = PmxNative.nativeLoadPmx(handle, pmxPath, dataDir);
            if (!ok) { ready = false; return; }

            PmxNative.nativeAddVmd(handle, "D:/예찬/MMD/motion/STAYC - Teddy Bear/STAYC - Teddy Bear/Teddy Bear.vmd");
            PmxNative.nativeUpdate(handle, 0.0f, 0.0f);

            allocateCpuBuffers();
            copyOnce();

            buildMaterials();
            buildSubmeshInfos();

            ready = true;
        } catch (Throwable t) {
            LOGGER.error("[PMX] init error", t);
            ready = false;
        }
    }

    public void shutdown() {
        if (handle != 0L) {
            try { PmxNative.nativeDestroy(handle); } catch (Throwable ignored) {}
            handle = 0L;
        }
        ready = false;
    }

    public void tick() {
        if (!ready || handle == 0L) return;

        long now = System.nanoTime();
        if (lastNanos < 0) lastNanos = now;
        float dt = (now - lastNanos) / 1_000_000_000.0f;
        lastNanos = now;

        frame += dt * 30.0f;
        PmxNative.nativeUpdate(handle, frame, dt);
    }

    public void syncCpuBuffersForRender() {
        if (!ready || handle == 0L) return;
        if (idxBuf == null) return;
        copyOnce();
    }

    private void allocateCpuBuffers() {
        int vertexCount = PmxNative.nativeGetVertexCount(handle);
        int indexCount  = PmxNative.nativeGetIndexCount(handle);
        int elemSize    = PmxNative.nativeGetIndexElementSize(handle);

        idxBuf = ByteBuffer.allocateDirect(indexCount * elemSize).order(ByteOrder.nativeOrder());
        posBuf = ByteBuffer.allocateDirect(vertexCount * 3 * 4).order(ByteOrder.nativeOrder());
        nrmBuf = ByteBuffer.allocateDirect(vertexCount * 3 * 4).order(ByteOrder.nativeOrder());
        uvBuf  = ByteBuffer.allocateDirect(vertexCount * 2 * 4).order(ByteOrder.nativeOrder());
    }

    private void copyOnce() {
        idxBuf.clear();
        posBuf.clear();
        nrmBuf.clear();
        uvBuf.clear();
        PmxNative.nativeCopyIndices(handle, idxBuf);
        PmxNative.nativeCopyPositions(handle, posBuf);
        PmxNative.nativeCopyNormals(handle, nrmBuf);
        PmxNative.nativeCopyUVs(handle, uvBuf);
    }

    private void buildSubmeshInfos() {
        int subCount = PmxNative.nativeGetSubmeshCount(handle);
        submeshes = new SubmeshInfo[subCount];

        for (int s = 0; s < subCount; s++) {
            int begin = PmxNative.nativeGetSubmeshBeginIndex(handle, s);
            int count = PmxNative.nativeGetSubmeshIndexCount(handle, s);
            int matId = PmxNative.nativeGetSubmeshMaterialId(handle, s);
            submeshes[s] = new SubmeshInfo(begin, count, matId);
        }
    }

    private void buildMaterials() {
        int matCount = PmxNative.nativeGetMaterialCount(handle);
        materials = new MaterialInfo[matCount];

        for (int m = 0; m < matCount; m++) {
            int diffuseRGBA = PmxNative.nativeGetMaterialDiffuseRGBA(handle, m);
            float alpha     = PmxNative.nativeGetMaterialAlpha(handle, m);

            final int mm1 = m;
            float[] ambient  = readVec3(dst -> PmxNative.nativeGetMaterialAmbient(handle, mm1, dst));
            float[] specular = readVec3(dst -> PmxNative.nativeGetMaterialSpecular(handle, mm1, dst));

            float specPow = PmxNative.nativeGetMaterialSpecularPower(handle, m);
            boolean bothFace = PmxNative.nativeGetMaterialBothFaceByMaterial(handle, m);

            String mainPath   = resolveTexturePathString(PmxNative.nativeGetMaterialTexturePath(handle, m));
            String toonPath   = resolveTexturePathString(PmxNative.nativeGetMaterialToonTexturePath(handle, m));
            String spherePath = resolveTexturePathString(PmxNative.nativeGetMaterialSphereTexturePath(handle, m));

            final int mm2 = m;
            float[] texMul    = readVec4(dst -> PmxNative.nativeGetMaterialTexMulFactor(handle, mm2, dst));
            float[] texAdd    = readVec4(dst -> PmxNative.nativeGetMaterialTexAddFactor(handle, mm2, dst));
            float[] toonMul   = readVec4(dst -> PmxNative.nativeGetMaterialToonMulFactor(handle, mm2, dst));
            float[] toonAdd   = readVec4(dst -> PmxNative.nativeGetMaterialToonAddFactor(handle, mm2, dst));
            float[] sphereMul = readVec4(dst -> PmxNative.nativeGetMaterialSphereMulFactor(handle, mm2, dst));
            float[] sphereAdd = readVec4(dst -> PmxNative.nativeGetMaterialSphereAddFactor(handle, mm2, dst));

            int sphereMode = PmxNative.nativeGetMaterialSphereMode(handle, m);

            int edgeFlag = PmxNative.nativeGetMaterialEdgeFlag(handle, m);
            float edgeSize = PmxNative.nativeGetMaterialEdgeSize(handle, m);
            final int mm3 = m;
            float[] edgeColor = readVec4(dst -> PmxNative.nativeGetMaterialEdgeColor(handle, mm3, dst));

            boolean groundShadow   = PmxNative.nativeGetMaterialGroundShadow(handle, m);
            boolean shadowCaster   = PmxNative.nativeGetMaterialShadowCaster(handle, m);
            boolean shadowReceiver = PmxNative.nativeGetMaterialShadowReceiver(handle, m);

            materials[m] = new MaterialInfo(
                    diffuseRGBA, alpha,

                    ambient[0], ambient[1], ambient[2],
                    specular[0], specular[1], specular[2],
                    specPow,
                    bothFace,

                    mainPath,
                    texMul, texAdd,

                    toonPath,
                    toonMul, toonAdd,

                    spherePath,
                    sphereMode,
                    sphereMul, sphereAdd,

                    edgeFlag != 0,
                    edgeSize,
                    edgeColor,

                    groundShadow,
                    shadowCaster,
                    shadowReceiver
            );
        }
    }

    private interface VecWriter3 { void write(ByteBuffer dst3f); }
    private interface VecWriter4 { void write(ByteBuffer dst4f); }

    private float[] readVec3(VecWriter3 w) {
        tmp3f.clear();
        w.write(tmp3f);
        tmp3f.rewind();
        return new float[]{ tmp3f.getFloat(), tmp3f.getFloat(), tmp3f.getFloat() };
    }

    private float[] readVec4(VecWriter4 w) {
        tmp4f.clear();
        w.write(tmp4f);
        tmp4f.rewind();
        return new float[]{ tmp4f.getFloat(), tmp4f.getFloat(), tmp4f.getFloat(), tmp4f.getFloat() };
    }

    private String resolveTexturePathString(String texPath) {
        if (texPath == null || texPath.isEmpty()) return null;
        try {
            Path p = Paths.get(texPath);
            if (p.isAbsolute()) return p.normalize().toString();
            if (pmxBaseDir != null) return pmxBaseDir.resolve(p).normalize().toString();
            return p.normalize().toString();
        } catch (Throwable ignored) {
            return null;
        }
    }

    public record SubmeshInfo(
            int beginIndex,
            int indexCount,
            int materialId
    ) {}

    public record MaterialInfo(
            int diffuseRGBA,
            float alpha,

            float ambientR, float ambientG, float ambientB,
            float specularR, float specularG, float specularB,
            float specularPower,
            boolean bothFace,

            String mainTexPath,
            float[] texMul, float[] texAdd,

            String toonTexPath,
            float[] toonMul, float[] toonAdd,

            String sphereTexPath,
            int sphereMode,
            float[] sphereMul, float[] sphereAdd,

            boolean edgeFlag,
            float edgeSize,
            float[] edgeColor,

            boolean groundShadow,
            boolean shadowCaster,
            boolean shadowReceiver
    ) {}
}
