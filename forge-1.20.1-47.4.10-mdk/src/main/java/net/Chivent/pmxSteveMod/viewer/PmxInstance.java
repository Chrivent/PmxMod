package net.Chivent.pmxSteveMod.viewer;

import com.mojang.logging.LogUtils;
import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.jni.PmxNative;
import net.minecraft.client.Minecraft;
import net.minecraft.resources.ResourceLocation;
import org.slf4j.Logger;

import java.io.InputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;

public class PmxInstance {
    private static final Logger LOGGER = LogUtils.getLogger();
    private static Path cachedToonDir;

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

    private boolean indicesCopiedOnce = false;

    public boolean isReady() { return ready; }
    public long handle() { return handle; }

    public ByteBuffer idxBuf() { return idxBuf; }
    public ByteBuffer posBuf() { return posBuf; }
    public ByteBuffer nrmBuf() { return nrmBuf; }
    public ByteBuffer uvBuf()  { return uvBuf;  }

    public SubmeshInfo[] submeshes() { return submeshes; }

    public MaterialInfo material(int id) {
        if (materials == null || id < 0 || id >= materials.length) return null;
        return materials[id];
    }

    public Path pmxBaseDir() { return pmxBaseDir; }

    public void init(Path pmxPath) {
        try {
            handle = PmxNative.nativeCreate();

            if (pmxPath == null || !Files.exists(pmxPath)) {
                ready = false;
                if (handle != 0L) {
                    try { PmxNative.nativeDestroy(handle); } catch (Throwable ignored) {}
                    handle = 0L;
                }
                return;
            }
            pmxBaseDir = pmxPath.getParent();
            Path dataDirPath = ensureToonDir();
            String dataDir = dataDirPath != null ? dataDirPath.toString() : "";

            boolean ok = PmxNative.nativeLoadPmx(handle, pmxPath.toString(), dataDir);
            if (!ok) { ready = false; return; }

            PmxNative.nativeAddVmd(handle, "D:/예찬/MMD/motion/STAYC - Teddy Bear/STAYC - Teddy Bear/Teddy Bear.vmd");
            PmxNative.nativeUpdate(handle, 0.0f, 0.0f);

            allocateCpuBuffers();

            copyIndicesOnce();
            copyDynamicVertices();

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

        indicesCopiedOnce = false;
        frame = 0f;
        lastNanos = -1;
    }

    public void tickRender() {
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
        copyIndicesOnce();
        if (posBuf == null || nrmBuf == null || uvBuf == null) return;
        copyDynamicVertices();
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

    private void copyIndicesOnce() {
        if (indicesCopiedOnce) return;
        if (idxBuf == null) return;

        idxBuf.clear();
        PmxNative.nativeCopyIndices(handle, idxBuf);

        indicesCopiedOnce = true;
    }

    private void copyDynamicVertices() {
        posBuf.clear();
        nrmBuf.clear();
        uvBuf.clear();

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

    private static Path ensureToonDir() {
        if (cachedToonDir != null) return cachedToonDir;
        Path outDir = Paths.get(System.getProperty("java.io.tmpdir"), "pmx_steve_mod", "toon");
        try {
            Files.createDirectories(outDir);
        } catch (IOException e) {
            LOGGER.warn("[PMX] failed to create toon cache dir", e);
            return null;
        }
        Minecraft mc = Minecraft.getInstance();
        for (int i = 1; i <= 10; i++) {
            String name = String.format("toon%02d.bmp", i);
            Path outFile = outDir.resolve(name);
            if (Files.exists(outFile)) continue;
            ResourceLocation id = ResourceLocation.fromNamespaceAndPath(
                    PmxSteveMod.MOD_ID,
                    "textures/pmx/" + name
            );
            try (InputStream in = mc.getResourceManager().getResource(id).orElseThrow().open()) {
                Files.copy(in, outFile, StandardCopyOption.REPLACE_EXISTING);
            } catch (Throwable t) {
                LOGGER.warn("[PMX] missing builtin toon {}", name, t);
            }
        }
        cachedToonDir = outDir;
        return cachedToonDir;
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
