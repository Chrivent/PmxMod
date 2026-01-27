package net.Chivent.pmxSteveMod.viewer;

import com.mojang.blaze3d.platform.NativeImage;
import com.mojang.blaze3d.shaders.Uniform;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.*;
import com.mojang.math.Axis;
import net.Chivent.pmxSteveMod.client.PmxShaders;
import net.Chivent.pmxSteveMod.viewer.PmxViewer.MaterialInfo;
import net.Chivent.pmxSteveMod.viewer.PmxViewer.SubmeshInfo;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.ShaderInstance;
import net.minecraft.client.renderer.texture.DynamicTexture;
import net.minecraft.client.renderer.texture.OverlayTexture;
import net.minecraft.client.renderer.texture.TextureManager;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import net.minecraft.util.Mth;
import org.joml.Matrix4f;

import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

public class PmxRenderer {
    private record TextureEntry(ResourceLocation rl, boolean hasAlpha) {}
    private final Map<String, TextureEntry> textureCache = new HashMap<>();
    private ResourceLocation magentaTex = null;

    private static final class MaterialGpu {
        int texMode;        // 0 none, 1 rgb, 2 rgba
        int toonMode;       // 0 none, 1 on
        int sphereMode;     // 0 none, 1 mul, 2 add

        boolean mainHasAlpha;

        ResourceLocation mainTex;
        ResourceLocation sphereTex;
        ResourceLocation toonTex;
    }

    private final Map<Integer, MaterialGpu> materialGpuCache = new HashMap<>();

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

    public void renderPlayer(PmxViewer viewer,
                             AbstractClientPlayer player,
                             float partialTick,
                             PoseStack poseStack,
                             int packedLight) {
        if (!viewer.isReady() || viewer.handle() == 0L) return;
        if (viewer.idxBuf() == null || viewer.posBuf() == null || viewer.nrmBuf() == null || viewer.uvBuf() == null) return;

        viewer.syncCpuBuffersForRender();

        poseStack.pushPose();

        float yRot = Mth.lerp(partialTick, player.yRotO, player.getYRot());
        poseStack.mulPose(Axis.YP.rotationDegrees(180.0f - yRot));
        poseStack.scale(0.15f, 0.15f, 0.15f);

        Matrix4f pose = poseStack.last().pose();

        int elemSize   = net.Chivent.pmxSteveMod.jni.PmxNative.nativeGetIndexElementSize(viewer.handle());
        int indexCount = net.Chivent.pmxSteveMod.jni.PmxNative.nativeGetIndexCount(viewer.handle());
        int vtxCount   = viewer.posBuf().capacity() / 12;

        SubmeshInfo[] subs = viewer.submeshes();
        if (subs == null) { poseStack.popPose(); return; }

        RenderSystem.enableDepthTest();
        RenderSystem.enableBlend();
        RenderSystem.defaultBlendFunc();
        RenderSystem.depthMask(true);

        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

        for (int s = 0; s < subs.length; s++)
            drawSubmesh(viewer, s, pose, packedLight, elemSize, indexCount, vtxCount);

        RenderSystem.depthMask(true);
        RenderSystem.disableBlend();
        RenderSystem.enableCull();
        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

        poseStack.popPose();
    }

    private void drawSubmesh(PmxViewer viewer,
                             int s,
                             Matrix4f pose,
                             int packedLight,
                             int elemSize,
                             int indexCount,
                             int vtxCount) {
        SubmeshInfo sm = viewer.submeshes()[s];
        MaterialInfo mat = viewer.material(sm.materialId());
        if (mat == null) return;

        int begin = sm.beginIndex();
        int count = sm.indexCount();
        if (begin < 0 || count <= 0) return;

        int maxCount = Math.max(0, indexCount - begin);
        if (count > maxCount) count = maxCount;
        count -= (count % 3);
        if (count <= 0) return;

        float alpha = mat.alpha();
        if (alpha <= 0.0f) return;

        MaterialGpu gpu = getOrBuildMaterialGpu(viewer, sm.materialId(), mat);

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

        set3f(sh, "u_Ambient", mat.ambientR(), mat.ambientG(), mat.ambientB());

        int rgba = mat.diffuseRGBA();
        float mr = ((rgba >>> 24) & 0xFF) / 255.0f;
        float mg = ((rgba >>> 16) & 0xFF) / 255.0f;
        float mb = ((rgba >>>  8) & 0xFF) / 255.0f;
        set3f(sh, "u_Diffuse", mr, mg, mb);

        set3f(sh, "u_Specular", mat.specularR(), mat.specularG(), mat.specularB());
        set1f(sh, "u_SpecularPower", mat.specularPower());
        set1f(sh, "u_Alpha", alpha);

        RenderSystem.setShaderTexture(0, gpu.mainTex);
        RenderSystem.setShaderTexture(1, gpu.sphereTex);
        RenderSystem.setShaderTexture(2, gpu.toonTex);

        set1i(sh, "u_TexMode", gpu.texMode);
        float[] tm = mat.texMul();
        float[] ta = mat.texAdd();
        set4f(sh, "u_TexMulFactor", tm[0], tm[1], tm[2], tm[3]);
        set4f(sh, "u_TexAddFactor", ta[0], ta[1], ta[2], ta[3]);

        set1i(sh, "u_SphereTexMode", gpu.sphereMode);
        float[] spMul = mat.sphereMul();
        float[] spAdd = mat.sphereAdd();
        set4f(sh, "u_SphereTexMulFactor", spMul[0], spMul[1], spMul[2], spMul[3]);
        set4f(sh, "u_SphereTexAddFactor", spAdd[0], spAdd[1], spAdd[2], spAdd[3]);

        set1i(sh, "u_ToonTexMode", gpu.toonMode);
        float[] toonMul = mat.toonMul();
        float[] toonAdd = mat.toonAdd();
        set4f(sh, "u_ToonTexMulFactor", toonMul[0], toonMul[1], toonMul[2], toonMul[3]);
        set4f(sh, "u_ToonTexAddFactor", toonAdd[0], toonAdd[1], toonAdd[2], toonAdd[3]);

        RenderSystem.enableDepthTest();
        RenderSystem.enableBlend();
        RenderSystem.defaultBlendFunc();
        RenderSystem.depthMask(true);

        if (mat.bothFace()) RenderSystem.disableCull();
        else RenderSystem.enableCull();

        Tesselator tess = Tesselator.getInstance();
        BufferBuilder bb = tess.getBuilder();
        bb.begin(VertexFormat.Mode.TRIANGLES, DefaultVertexFormat.NEW_ENTITY);

        int triCount = count / 3;
        for (int t = 0; t < triCount; t++) {
            int ii0 = begin + t * 3;
            int ii1 = begin + t * 3 + 1;
            int ii2 = begin + t * 3 + 2;

            int i0 = readIndex(viewer.idxBuf(), elemSize, ii0);
            int i1 = readIndex(viewer.idxBuf(), elemSize, ii1);
            int i2 = readIndex(viewer.idxBuf(), elemSize, ii2);

            if (i0 < 0 || i1 < 0 || i2 < 0) continue;
            if (i0 >= vtxCount || i1 >= vtxCount || i2 >= vtxCount) continue;

            putVertex(viewer, bb, packedLight, i0);
            putVertex(viewer, bb, packedLight, i1);
            putVertex(viewer, bb, packedLight, i2);
        }

        BufferUploader.drawWithShader(bb.end());
        RenderSystem.enableCull();
    }

    private void putVertex(PmxViewer viewer, BufferBuilder bb, int packedLight, int vi) {
        ByteBuffer posBuf = viewer.posBuf();
        ByteBuffer nrmBuf = viewer.nrmBuf();
        ByteBuffer uvBuf  = viewer.uvBuf();

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

        if (viewer.flipV()) v = 1.0f - v;

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

    private MaterialGpu getOrBuildMaterialGpu(PmxViewer viewer, int materialId, MaterialInfo mat) {
        MaterialGpu cached = materialGpuCache.get(materialId);
        if (cached != null) return cached;

        MaterialGpu gpu = new MaterialGpu();

        TextureEntry main = getOrLoadTextureEntry(viewer, mat.mainTexPath());
        if (main != null && main.rl != null) {
            gpu.mainTex = main.rl;
            gpu.mainHasAlpha = main.hasAlpha;
            gpu.texMode = main.hasAlpha ? 2 : 1;
        } else {
            gpu.mainTex = ensureMagentaTexture();
            gpu.mainHasAlpha = false;
            gpu.texMode = 0;
        }

        TextureEntry sphere = getOrLoadTextureEntry(viewer, mat.sphereTexPath());
        if (sphere != null && sphere.rl != null) {
            gpu.sphereTex = sphere.rl;
            gpu.sphereMode = mat.sphereMode(); // 0/1/2
        } else {
            gpu.sphereTex = ensureMagentaTexture();
            gpu.sphereMode = 0;
        }

        TextureEntry toon = getOrLoadTextureEntry(viewer, mat.toonTexPath());
        if (toon != null && toon.rl != null) {
            gpu.toonTex = toon.rl;
            gpu.toonMode = 1;
        } else {
            gpu.toonTex = ensureMagentaTexture();
            gpu.toonMode = 0;
        }

        materialGpuCache.put(materialId, gpu);
        return gpu;
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
            return null;
        }
    }

    private TextureEntry getOrLoadTextureEntry(PmxViewer viewer, String texPath) {
        if (texPath == null || texPath.isEmpty()) return null;

        Path resolved = resolveTexturePath(viewer, texPath);
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
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static Path resolveTexturePath(PmxViewer viewer, String texPath) {
        try {
            Path p = Paths.get(texPath);
            if (p.isAbsolute()) return p.normalize();
            Path base = viewer.pmxBaseDir();
            if (base != null) return base.resolve(p).normalize();
            return p.normalize();
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static boolean imageHasAnyAlpha(NativeImage img) {
        try {
            int w = img.getWidth();
            int h = img.getHeight();
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int rgba = img.getPixelRGBA(x, y);
                    int a = (rgba >>> 24) & 0xFF;
                    if (a != 0xFF) return true;
                }
            }
        } catch (Throwable ignored) {}
        return false;
    }
}
