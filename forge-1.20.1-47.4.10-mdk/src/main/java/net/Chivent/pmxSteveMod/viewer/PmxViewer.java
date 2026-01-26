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
    private final Map<String, ResourceLocation> textureCache = new HashMap<>();
    private ResourceLocation magentaTex = null;

    private SubmeshInfo[] submeshes;

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
            int count = PmxNative.nativeGetSubmeshVertexCount(handle, s);
            int rgba  = PmxNative.nativeGetSubmeshDiffuseRGBA(handle, s);
            float alphaMat = PmxNative.nativeGetSubmeshAlpha(handle, s);
            boolean bothFace = PmxNative.nativeGetSubmeshBothFace(handle, s);

            String texPath = PmxNative.nativeGetSubmeshTexturePath(handle, s);
            ResourceLocation tex = getOrLoadTexture(texPath);
            boolean hasMainTex = (texPath != null && !texPath.isEmpty());

            submeshes[s] = new SubmeshInfo(begin, count, rgba, alphaMat, bothFace, tex, hasMainTex);
        }
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

    private ResourceLocation getOrLoadTexture(String texPath) {
        if (texPath == null || texPath.isEmpty()) return ensureMagentaTexture();
        Path resolved = resolveTexturePath(texPath);
        if (resolved == null) return ensureMagentaTexture();

        String key = resolved.toString();
        ResourceLocation cached = textureCache.get(key);
        if (cached != null) return cached;

        try (InputStream in = Files.newInputStream(resolved)) {
            NativeImage img = NativeImage.read(in);
            DynamicTexture dt = new DynamicTexture(img);
            TextureManager tm = Minecraft.getInstance().getTextureManager();
            String id = "pmx/" + Integer.toHexString(key.hashCode());
            ResourceLocation rl = tm.register(id, dt);
            textureCache.put(key, rl);
            return rl;
        } catch (Throwable t) {
            LOGGER.warn("[PMX] texture load failed: {}", key, t);
            return ensureMagentaTexture();
        }
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
            int diffuseRGBA,
            float alphaMat,
            boolean bothFace,
            ResourceLocation mainTex,
            boolean hasMainTex
    ) {}
}
