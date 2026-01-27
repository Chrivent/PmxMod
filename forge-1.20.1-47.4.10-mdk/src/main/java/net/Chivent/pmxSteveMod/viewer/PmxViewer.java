package net.Chivent.pmxSteveMod.viewer;

import com.mojang.blaze3d.platform.NativeImage;
import com.mojang.logging.LogUtils;
import net.Chivent.pmxSteveMod.jni.PmxNative;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.texture.DynamicTexture;
import net.minecraft.client.renderer.texture.TextureManager;
import net.minecraft.resources.ResourceLocation;
import org.slf4j.Logger;

import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

public class PmxViewer {
    private static final Logger LOGGER = LogUtils.getLogger();
    private static final PmxViewer INSTANCE = new PmxViewer();
    public static PmxViewer get() { return INSTANCE; }

    private long handle = 0L;
    private boolean ready = false;

    private float frame = 0f;
    private long lastNanos = -1;

    private ByteBuffer idxBuf;
    private ByteBuffer posBuf;
    private ByteBuffer nrmBuf;
    private ByteBuffer uvBuf;

    private Path pmxBaseDir = null;

    private static final class TextureEntry {
        final ResourceLocation rl;
        final boolean hasAlpha;
        TextureEntry(ResourceLocation rl, boolean hasAlpha) {
            this.rl = rl;
            this.hasAlpha = hasAlpha;
        }
    }

    private final Map<String, TextureEntry> textureCache = new HashMap<>();
    private ResourceLocation magentaTex = null;

    private SubmeshInfo[] submeshes;
    private MaterialInfo[] materials;

    // native vec3/vec4 읽기용 임시 버퍼 (direct + native order)
    private final ByteBuffer tmp3f = ByteBuffer.allocateDirect(3 * 4).order(ByteOrder.nativeOrder());
    private final ByteBuffer tmp4f = ByteBuffer.allocateDirect(4 * 4).order(ByteOrder.nativeOrder());

    private final PmxRenderer renderer = new PmxRenderer();
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
    public ResourceLocation magentaTex() { return magentaTex; }

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

            ensureMagentaTexture();

            buildMaterials();      // ★ 추가
            buildSubmeshInfos();   // materialId만 들고 있도록

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

            // vec3
            final int m1 = m;
            float[] ambient  = readVec3((dst) -> PmxNative.nativeGetMaterialAmbient(handle, m1, dst));
            float[] specular = readVec3((dst) -> PmxNative.nativeGetMaterialSpecular(handle, m1, dst));

            float specPow = PmxNative.nativeGetMaterialSpecularPower(handle, m);
            boolean bothFace = PmxNative.nativeGetMaterialBothFaceByMaterial(handle, m);

            // textures
            String mainPath  = PmxNative.nativeGetMaterialTexturePath(handle, m);
            String toonPath  = PmxNative.nativeGetMaterialToonTexturePath(handle, m);
            String spherePath= PmxNative.nativeGetMaterialSphereTexturePath(handle, m);

            TextureEntry main   = getOrLoadTextureEntry(mainPath);
            TextureEntry toon   = getOrLoadTextureEntry(toonPath);
            TextureEntry sphere = getOrLoadTextureEntry(spherePath);

            // factors vec4
            final int m2 = m;
            float[] texMul    = readVec4(dst -> PmxNative.nativeGetMaterialTexMulFactor(handle, m2, dst));
            float[] texAdd    = readVec4(dst -> PmxNative.nativeGetMaterialTexAddFactor(handle, m2, dst));
            float[] toonMul   = readVec4(dst -> PmxNative.nativeGetMaterialToonMulFactor(handle, m2, dst));
            float[] toonAdd   = readVec4(dst -> PmxNative.nativeGetMaterialToonAddFactor(handle, m2, dst));
            float[] sphereMul = readVec4(dst -> PmxNative.nativeGetMaterialSphereMulFactor(handle, m2, dst));
            float[] sphereAdd = readVec4(dst -> PmxNative.nativeGetMaterialSphereAddFactor(handle, m2, dst));

            int sphereMode = PmxNative.nativeGetMaterialSphereMode(handle, m); // 0/1/2

            // edge/shadow (너가 추가한 6개)
            int edgeFlag = PmxNative.nativeGetMaterialEdgeFlag(handle, m);
            float edgeSize = PmxNative.nativeGetMaterialEdgeSize(handle, m);
            final int m3 = m;
            float[] edgeColor = readVec4((dst) -> PmxNative.nativeGetMaterialEdgeColor(handle, m3, dst));

            boolean groundShadow = PmxNative.nativeGetMaterialGroundShadow(handle, m);
            boolean shadowCaster = PmxNative.nativeGetMaterialShadowCaster(handle, m);
            boolean shadowReceiver = PmxNative.nativeGetMaterialShadowReceiver(handle, m);

            // 원본 로직: mainTex 있으면 texMode=1(no alpha) or 2(has alpha), 없으면 0
            int texMode;
            ResourceLocation mainRL = ensureMagentaTexture();
            boolean mainHasAlpha = false;
            boolean hasMainTex = (mainPath != null && !mainPath.isEmpty() && main != null && main.rl != null);
            if (hasMainTex) {
                mainRL = main.rl;
                mainHasAlpha = main.hasAlpha;
                texMode = mainHasAlpha ? 2 : 1;
            } else {
                texMode = 0;
            }

            int toonMode = (toonPath != null && !toonPath.isEmpty() && toon != null && toon.rl != null) ? 1 : 0;
            ResourceLocation toonRL = (toonMode != 0) ? toon.rl : ensureMagentaTexture();

            int spMode = (spherePath != null && !spherePath.isEmpty() && sphere != null && sphere.rl != null) ? sphereMode : 0;
            ResourceLocation sphereRL = (spMode != 0) ? sphere.rl : ensureMagentaTexture();

            materials[m] = new MaterialInfo(
                    diffuseRGBA, alpha,
                    ambient[0], ambient[1], ambient[2],
                    specular[0], specular[1], specular[2],
                    specPow,
                    bothFace,

                    texMode, mainRL,
                    texMul, texAdd,

                    toonMode, toonRL,
                    toonMul, toonAdd,

                    spMode, sphereRL,
                    sphereMul, sphereAdd,

                    edgeFlag != 0, edgeSize, edgeColor,
                    groundShadow, shadowCaster, shadowReceiver
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

    private ResourceLocation ensureMagentaTexture() {
        if (magentaTex != null) return magentaTex;
        try {
            NativeImage img = new NativeImage(1, 1, false);
            img.setPixelRGBA(0, 0, 0xFFFF00FF);
            DynamicTexture dt = new DynamicTexture(img);
            TextureManager tm = Minecraft.getInstance().getTextureManager();
            magentaTex = tm.register("pmx/magenta", dt);
            return magentaTex;
        } catch (Throwable t) {
            LOGGER.error("[PMX] failed to create magenta texture", t);
            return null;
        }
    }

    private TextureEntry getOrLoadTextureEntry(String texPath) {
        if (texPath == null || texPath.isEmpty()) return null;
        Path resolved = resolveTexturePath(texPath);
        if (resolved == null) return null;

        String key = resolved.toString();
        TextureEntry cached = textureCache.get(key);
        if (cached != null) return cached;

        try (InputStream in = Files.newInputStream(resolved)) {
            NativeImage img = NativeImage.read(in);
            boolean hasAlpha = imageHasAnyAlpha(img);

            DynamicTexture dt = new DynamicTexture(img);
            TextureManager tm = Minecraft.getInstance().getTextureManager();
            String id = "pmx/" + Integer.toHexString(key.hashCode());
            ResourceLocation rl = tm.register(id, dt);

            TextureEntry e = new TextureEntry(rl, hasAlpha);
            textureCache.put(key, e);
            return e;
        } catch (Throwable t) {
            LOGGER.warn("[PMX] texture load failed: {}", key, t);
            return null;
        }
    }

    private static boolean imageHasAnyAlpha(NativeImage img) {
        // 빠른 early-out 스캔: 알파가 255가 아닌 픽셀이 하나라도 있으면 true
        try {
            int w = img.getWidth();
            int h = img.getHeight();
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int rgba = img.getPixelRGBA(x, y);
                    int a = (rgba >>> 24) & 0xFF; // NativeImage는 보통 ABGR이지만,
                    // 여기서는 "알파 채널 존재/사용 여부"만 보고 싶어서 상위 바이트만 검사 (대부분 맞음)
                    if (a != 0xFF) return true;
                }
            }
        } catch (Throwable ignored) {}
        return false;
    }

    private Path resolveTexturePath(String texPath) {
        try {
            Path p = Paths.get(texPath);
            if (p.isAbsolute()) return p.normalize();
            if (pmxBaseDir != null) return pmxBaseDir.resolve(p).normalize();
            return p.normalize();
        } catch (Throwable ignored) {
            return null;
        }
    }

    public PmxRenderer renderer() { return renderer; }

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

            int texMode,
            ResourceLocation mainTex,
            float[] texMul,
            float[] texAdd,

            int toonTexMode,
            ResourceLocation toonTex,
            float[] toonMul,
            float[] toonAdd,

            int sphereTexMode,
            ResourceLocation sphereTex,
            float[] sphereMul,
            float[] sphereAdd,

            boolean edgeFlag,
            float edgeSize,
            float[] edgeColor,

            boolean groundShadow,
            boolean shadowCaster,
            boolean shadowReceiver
    ) {}
}
