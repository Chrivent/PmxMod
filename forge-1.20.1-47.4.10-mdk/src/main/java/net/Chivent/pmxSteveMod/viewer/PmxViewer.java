package net.Chivent.pmxSteveMod.viewer;

import com.mojang.blaze3d.platform.NativeImage;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.BufferBuilder;
import com.mojang.blaze3d.vertex.BufferUploader;
import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.Tesselator;
import com.mojang.blaze3d.vertex.VertexFormat;
import com.mojang.logging.LogUtils;
import com.mojang.math.Axis;

import net.Chivent.pmxSteveMod.jni.PmxNative;

import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.client.renderer.entity.player.PlayerRenderer;
import net.minecraft.client.renderer.texture.DynamicTexture;
import net.minecraft.client.renderer.texture.OverlayTexture;
import net.minecraft.client.renderer.texture.TextureManager;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import net.minecraft.util.Mth;

import org.joml.Matrix4f;
import org.slf4j.Logger;

import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.*;

import net.Chivent.pmxSteveMod.client.PmxShaders;
import net.minecraft.client.renderer.ShaderInstance;
import com.mojang.blaze3d.shaders.Uniform;

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
    private ResourceLocation[] submeshTex;

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

            int subCount = PmxNative.nativeGetSubmeshCount(handle);
            submeshTex = new ResourceLocation[subCount];
            for (int s = 0; s < subCount; s++) {
                String texPath = PmxNative.nativeGetSubmeshTexturePath(handle, s);
                submeshTex[s] = getOrLoadTexture(texPath);
            }

            ensureMagentaTexture();
            ready = true;
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

        poseStack.pushPose();

        float yRot = Mth.lerp(partialTick, player.yRotO, player.getYRot());
        poseStack.mulPose(Axis.YP.rotationDegrees(180.0f - yRot));
        poseStack.scale(0.15f, 0.15f, 0.15f);

        Matrix4f pose = poseStack.last().pose();

        int elemSize   = PmxNative.nativeGetIndexElementSize(handle);
        int indexCount = PmxNative.nativeGetIndexCount(handle);
        int vtxCount   = posBuf.capacity() / 12;

        int subCount = PmxNative.nativeGetSubmeshCount(handle);

        List<Integer> opaquePass = new ArrayList<>();
        List<Integer> translucentPass = new ArrayList<>();

        for (int s = 0; s < subCount; s++) {
            float alphaMat = PmxNative.nativeGetSubmeshAlpha(handle, s);
            int rgba = PmxNative.nativeGetSubmeshDiffuseRGBA(handle, s);
            float a = (rgba & 0xFF) / 255.0f;
            a *= alphaMat;
            if (a < 0.999f) translucentPass.add(s);
            else opaquePass.add(s);
        }

        RenderSystem.enableDepthTest();
        RenderSystem.depthMask(true);
        RenderSystem.disableBlend();
        RenderSystem.defaultBlendFunc();
        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

        for (int s : opaquePass) {
            drawSubmesh(s, false, pose, packedLight, elemSize, indexCount, vtxCount);
        }
        for (int s : translucentPass) {
            drawSubmesh(s, true, pose, packedLight, elemSize, indexCount, vtxCount);
        }

        RenderSystem.depthMask(true);
        RenderSystem.disableBlend();
        RenderSystem.enableCull();
        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

        poseStack.popPose();
    }

    private void setMat4(ShaderInstance sh, String name, Matrix4f m) {
        Uniform u = sh.getUniform(name);
        if (u != null) u.set(m);
    }
    private void set3f(ShaderInstance sh, String name, float x, float y, float z) {
        Uniform u = sh.getUniform(name);
        if (u != null) u.set(x, y, z);
    }
    private void set4f(ShaderInstance sh, String name, float x, float y, float z, float w) {
        Uniform u = sh.getUniform(name);
        if (u != null) u.set(x, y, z, w);
    }
    private void set1f(ShaderInstance sh, String name, float v) {
        Uniform u = sh.getUniform(name);
        if (u != null) u.set(v);
    }
    private void set1i(ShaderInstance sh, String name, int v) {
        Uniform u = sh.getUniform(name);
        if (u != null) u.set(v);
    }

    private void drawSubmesh(int s,
                             boolean forceTranslucentPass,
                             Matrix4f pose,
                             int packedLight,
                             int elemSize,
                             int indexCount,
                             int vtxCount) {

        int begin = PmxNative.nativeGetSubmeshBeginIndex(handle, s);
        int count = PmxNative.nativeGetSubmeshVertexCount(handle, s);
        if (begin < 0 || count <= 0) return;

        int maxCount = Math.max(0, indexCount - begin);
        if (count > maxCount) count = maxCount;
        count -= (count % 3);
        if (count <= 0) return;

        float alphaMat = PmxNative.nativeGetSubmeshAlpha(handle, s);
        int rgba = PmxNative.nativeGetSubmeshDiffuseRGBA(handle, s);
        boolean bothFace = PmxNative.nativeGetSubmeshBothFace(handle, s);

        float mr = ((rgba >>> 24) & 0xFF) / 255.0f;
        float mg = ((rgba >>> 16) & 0xFF) / 255.0f;
        float mb = ((rgba >>>  8) & 0xFF) / 255.0f;
        float ma = ( rgba         & 0xFF) / 255.0f;
        ma *= alphaMat;

        boolean translucent = forceTranslucentPass || (ma < 0.999f);

        ShaderInstance sh = PmxShaders.PMX_MMD;
        if (sh == null) return;

        Matrix4f proj = RenderSystem.getProjectionMatrix();
        Matrix4f wv  = new Matrix4f(pose);
        Matrix4f wvp = new Matrix4f(proj).mul(pose);

        RenderSystem.setShader(() -> sh);

        setMat4(sh, "u_WV", wv);
        setMat4(sh, "u_WVP", wvp);

        set3f(sh, "u_LightColor", 1f, 1f, 1f);
        set3f(sh, "u_LightDir", 0.2f, 1.0f, 0.2f);

        set3f(sh, "u_Ambient", 0f, 0f, 0f);
        set3f(sh, "u_Diffuse", mr, mg, mb);
        set3f(sh, "u_Specular", 0f, 0f, 0f);
        set1f(sh, "u_SpecularPower", 0f);
        set1f(sh, "u_Alpha", ma);

        final ResourceLocation mainTex = (submeshTex[s] != null) ? submeshTex[s] : magentaTex;
        RenderSystem.setShaderTexture(0, mainTex);
        RenderSystem.setShaderTexture(1, magentaTex);
        RenderSystem.setShaderTexture(2, magentaTex);

        boolean hasMainTex = (submeshTex[s] != null);
        set1i(sh, "u_TexMode", hasMainTex ? 2 : 0);

        set4f(sh, "u_TexMulFactor", 1f, 1f, 1f, 1f);
        set4f(sh, "u_TexAddFactor", 0f, 0f, 0f, 0f);

        set1i(sh, "u_SphereTexMode", 0);
        set1i(sh, "u_ToonTexMode", 0);

        RenderSystem.enableDepthTest();

        if (translucent) {
            RenderSystem.enableBlend();
            RenderSystem.defaultBlendFunc();
            RenderSystem.depthMask(false);
        } else {
            RenderSystem.disableBlend();
            RenderSystem.depthMask(true);
        }

        if (bothFace) RenderSystem.disableCull();
        else RenderSystem.enableCull();

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

            putVertex(bb, packedLight, i0);
            putVertex(bb, packedLight, i1);
            putVertex(bb, packedLight, i2);
        }

        BufferUploader.drawWithShader(bb.end());

        RenderSystem.depthMask(true);
        RenderSystem.disableBlend();
        RenderSystem.enableCull();
    }

    private void putVertex(BufferBuilder bb, int packedLight, int vi) {
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

        bb.vertex(x, y, z)
                .color(1f, 1f, 1f, 1f)
                .uv(u, v)
                .overlayCoords(OverlayTexture.NO_OVERLAY)
                .uv2(packedLight)
                .normal(nx, ny, nz)
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

    public void onReload() {}

    public void shutdown() {
        if (handle != 0L) {
            try { PmxNative.nativeDestroy(handle); } catch (Throwable ignored) {}
            handle = 0L;
        }
        ready = false;
    }
}
