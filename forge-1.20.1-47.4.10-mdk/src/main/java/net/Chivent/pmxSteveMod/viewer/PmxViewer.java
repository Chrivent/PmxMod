package net.Chivent.pmxSteveMod.viewer;

import com.mojang.blaze3d.platform.NativeImage;
import com.mojang.blaze3d.platform.Lighting;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.BufferBuilder;
import com.mojang.blaze3d.vertex.BufferUploader;
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.Tesselator;
import com.mojang.blaze3d.vertex.VertexFormat;
import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import com.mojang.logging.LogUtils;
import com.mojang.math.Axis;

import net.Chivent.pmxSteveMod.jni.PmxNative;

import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.client.renderer.entity.player.PlayerRenderer;
import net.minecraft.client.renderer.texture.DynamicTexture;
import net.minecraft.client.renderer.texture.OverlayTexture;
import net.minecraft.client.renderer.texture.TextureManager;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.client.renderer.LightTexture;

import net.minecraft.resources.ResourceLocation;
import net.minecraft.util.Mth;

import org.joml.Matrix3f;
import org.joml.Matrix4f;
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
    private ResourceLocation whiteTex = null;

    private static final boolean FLIP_V = true;

    private PmxViewer() {}

    public boolean isReady() { return ready; }

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

    public void init() {
        try {
            LOGGER.info("[PMX] before nativeCreate");
            handle = PmxNative.nativeCreate();
            LOGGER.info("[PMX] after nativeCreate handle={}", handle);

            String dataDir = "C:/Users/Ha Yechan/Desktop/PmxMod/resource/mmd";
            String pmxPath = "D:/예찬/MMD/model/Booth/Chrivent Elf/Chrivent Elf.pmx";
            pmxBaseDir = Paths.get(pmxPath).getParent();

            boolean ok = PmxNative.nativeLoadPmx(handle, pmxPath, dataDir);
            LOGGER.info("[PMX] nativeLoadPmx ok={}", ok);
            if (!ok) { ready = false; return; }

            boolean vmdOk = PmxNative.nativeAddVmd(handle,
                    "D:/예찬/MMD/motion/STAYC - Teddy Bear/STAYC - Teddy Bear/Teddy Bear.vmd");
            LOGGER.info("[PMX] nativeAddVmd ok={}", vmdOk);

            PmxNative.nativeUpdate(handle, 0.0f, 0.0f);

            allocateCpuBuffers();
            copyOnce();

            ensureWhiteTexture();
            ready = true;

        } catch (UnsatisfiedLinkError e) {
            LOGGER.error("[PMX] Native link error: {}", e.getMessage());
            ready = false;
        } catch (Throwable t) {
            LOGGER.error("[PMX] init error", t);
            ready = false;
        }
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

    public void renderPlayer(AbstractClientPlayer player,
                             PlayerRenderer vanillaRenderer,
                             float partialTick,
                             PoseStack poseStack,
                             MultiBufferSource buffers,
                             int packedLight) {
        if (!ready || handle == 0L) return;
        if (idxBuf == null || posBuf == null || nrmBuf == null || uvBuf == null) return;

        copyOnce();

        // ✅ (핵심) 직접 BufferUploader 쓰면 바닐라가 잡아주던 조명 상태가 빠질 수 있음
        Lighting.setupForEntityInInventory();
        Minecraft.getInstance().gameRenderer.lightTexture().turnOnLightLayer();
        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

        // packedLight가 0으로 들어오면 결과가 새까맣게 나올 수 있음 → fallback
        int light = packedLight;
        if (light == 0) light = LightTexture.pack(15, 15);

        poseStack.pushPose();

        float yRot = Mth.lerp(partialTick, player.yRotO, player.getYRot());
        poseStack.mulPose(Axis.YP.rotationDegrees(180.0f - yRot));

        float scale = 0.15f;
        poseStack.scale(scale, scale, scale);

        Matrix4f pose = poseStack.last().pose();
        Matrix3f normalMat = poseStack.last().normal();

        int elemSize   = PmxNative.nativeGetIndexElementSize(handle);
        int indexCount = PmxNative.nativeGetIndexCount(handle);
        int vtxCount   = posBuf.capacity() / 12;

        int subCount = PmxNative.nativeGetSubmeshCount(handle);

        for (int s = 0; s < subCount; s++) {
            int begin = PmxNative.nativeGetSubmeshBeginIndex(handle, s);
            int count = PmxNative.nativeGetSubmeshVertexCount(handle, s);
            if (begin < 0 || count <= 0) continue;

            // 범위 clamp + TRIANGLES 정렬
            int maxCount = Math.max(0, indexCount - begin);
            if (count > maxCount) count = maxCount;
            count -= (count % 3);
            if (count <= 0) continue;

            String texPath = PmxNative.nativeGetSubmeshTexturePath(handle, s);
            float alpha = PmxNative.nativeGetSubmeshAlpha(handle, s);
            int rgba = PmxNative.nativeGetSubmeshDiffuseRGBA(handle, s);
            boolean bothFace = PmxNative.nativeGetSubmeshBothFace(handle, s);

            ResourceLocation tex = getOrLoadTexture(texPath);
            if (tex == null) tex = ensureWhiteTexture();

            float r = ((rgba >>> 24) & 0xFF) / 255.0f;
            float g = ((rgba >>> 16) & 0xFF) / 255.0f;
            float b = ((rgba >>>  8) & 0xFF) / 255.0f;
            float a = ( rgba         & 0xFF) / 255.0f;
            a *= alpha;

            boolean translucent = a < 0.999f;

            // 셰이더 선택 (가능하면 타입에 맞는 걸 씀)
            if (translucent) {
                RenderSystem.setShader(GameRenderer::getRendertypeEntityTranslucentShader);
                RenderSystem.enableBlend();
                RenderSystem.defaultBlendFunc();
            } else {
                RenderSystem.setShader(GameRenderer::getRendertypeEntityCutoutShader);
                RenderSystem.disableBlend();
            }

            RenderSystem.setShaderTexture(0, tex);
            RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

            if (bothFace) RenderSystem.disableCull();
            else RenderSystem.enableCull();

            // TRIANGLES로 직접 빌드
            Tesselator tess = Tesselator.getInstance();
            BufferBuilder bb = tess.getBuilder();
            bb.begin(VertexFormat.Mode.TRIANGLES, DefaultVertexFormat.NEW_ENTITY);

            int triCount = count / 3;
            for (int t = 0; t < triCount; t++) {
                int ii0 = begin + t * 3;
                int ii1 = begin + t * 3 + 1;
                int ii2 = begin + t * 3 + 2;

                int i0 = readIndex(idxBuf, elemSize, ii0);
                int i1 = readIndex(idxBuf, elemSize, ii1);
                int i2 = readIndex(idxBuf, elemSize, ii2);

                if (i0 < 0 || i1 < 0 || i2 < 0) continue;
                if (i0 >= vtxCount || i1 >= vtxCount || i2 >= vtxCount) continue;

                putVertex(bb, pose, normalMat, light, r, g, b, a, i0);
                putVertex(bb, pose, normalMat, light, r, g, b, a, i1);
                putVertex(bb, pose, normalMat, light, r, g, b, a, i2);
            }

            BufferUploader.drawWithShader(bb.end());

            // 상태 복구
            RenderSystem.enableCull();
            RenderSystem.disableBlend();
        }

        poseStack.popPose();
    }

    private void putVertex(BufferBuilder bb,
                           Matrix4f pose,
                           Matrix3f normalMat,
                           int packedLight,
                           float r, float g, float b, float a,
                           int vi) {
        int pb = vi * 12;
        float x = posBuf.getFloat(pb);
        float y = posBuf.getFloat(pb + 4);
        float z = posBuf.getFloat(pb + 8);

        int nb = vi * 12;
        float nx = nrmBuf.getFloat(nb);
        float ny = nrmBuf.getFloat(nb + 4);
        float nz = nrmBuf.getFloat(nb + 8);

        int ub = vi * 8;
        float u = uvBuf.getFloat(ub);
        float v = uvBuf.getFloat(ub + 4);
        if (FLIP_V) v = 1.0f - v;

        bb.vertex(pose, x, y, z)
                .color(r, g, b, a)
                .uv(u, v)
                .overlayCoords(OverlayTexture.NO_OVERLAY)
                .uv2(packedLight)
                .normal(normalMat, nx, ny, nz)
                .endVertex();
    }

    private static int readIndex(ByteBuffer buf, int elemSize, int idx) {
        int bytePos = idx * elemSize;
        if (bytePos < 0 || bytePos + elemSize > buf.capacity()) return -1;

        return switch (elemSize) {
            case 1 -> (buf.get(bytePos) & 0xFF);
            case 2 -> (buf.getShort(bytePos) & 0xFFFF);
            case 4 -> buf.getInt(bytePos);
            default -> -1;
        };
    }

    // --- texture loading ---

    private ResourceLocation ensureWhiteTexture() {
        if (whiteTex != null) return whiteTex;
        try {
            NativeImage img = new NativeImage(1, 1, false);
            img.setPixelRGBA(0, 0, 0xFFFFFFFF);
            DynamicTexture dt = new DynamicTexture(img);
            TextureManager tm = Minecraft.getInstance().getTextureManager();
            whiteTex = tm.register("pmx/white", dt);
            return whiteTex;
        } catch (Throwable t) {
            LOGGER.error("[PMX] failed to create white texture", t);
            return null;
        }
    }

    private ResourceLocation getOrLoadTexture(String texPath) {
        if (texPath == null || texPath.isEmpty()) return ensureWhiteTexture();

        Path resolved = resolveTexturePath(texPath);
        if (resolved == null) return ensureWhiteTexture();

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
            LOGGER.warn("[PMX] texture load failed: {}", key);
            return ensureWhiteTexture();
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

    public void onReload() {}

    public void shutdown() {
        if (handle != 0L) {
            try { PmxNative.nativeDestroy(handle); } catch (Throwable ignored) {}
            handle = 0L;
        }
        ready = false;
    }
}
