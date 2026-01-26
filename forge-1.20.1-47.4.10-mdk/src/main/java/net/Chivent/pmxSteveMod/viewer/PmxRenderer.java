package net.Chivent.pmxSteveMod.viewer;

import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.*;
import com.mojang.math.Axis;
import net.Chivent.pmxSteveMod.client.PmxShaders;
import net.Chivent.pmxSteveMod.viewer.PmxViewer.SubmeshInfo;
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.client.renderer.ShaderInstance;
import net.minecraft.client.renderer.entity.player.PlayerRenderer;
import net.minecraft.client.renderer.texture.OverlayTexture;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import net.minecraft.util.Mth;
import org.joml.Matrix4f;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

import com.mojang.blaze3d.shaders.Uniform;

public class PmxRenderer {
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
                             PlayerRenderer vanillaRenderer,
                             float partialTick,
                             PoseStack poseStack,
                             MultiBufferSource buffers,
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

        List<Integer> opaquePass = new ArrayList<>();
        List<Integer> translucentPass = new ArrayList<>();

        for (int s = 0; s < subs.length; s++) {
            float a = calcFinalAlpha(subs[s]);
            if (a < 0.999f) translucentPass.add(s);
            else opaquePass.add(s);
        }

        RenderSystem.enableDepthTest();
        RenderSystem.depthMask(true);
        RenderSystem.disableBlend();
        RenderSystem.defaultBlendFunc();
        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

        for (int s : opaquePass) {
            drawSubmesh(viewer, s, false, pose, packedLight, elemSize, indexCount, vtxCount);
        }
        for (int s : translucentPass) {
            drawSubmesh(viewer, s, true, pose, packedLight, elemSize, indexCount, vtxCount);
        }

        RenderSystem.depthMask(true);
        RenderSystem.disableBlend();
        RenderSystem.enableCull();
        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

        poseStack.popPose();
    }

    private float calcFinalAlpha(SubmeshInfo sm) {
        int rgba = sm.diffuseRGBA();
        float ma = (rgba & 0xFF) / 255.0f;
        ma *= sm.alphaMat();
        return ma;
    }

    private void drawSubmesh(PmxViewer viewer,
                             int s,
                             boolean forceTranslucentPass,
                             Matrix4f pose,
                             int packedLight,
                             int elemSize,
                             int indexCount,
                             int vtxCount) {
        SubmeshInfo sm = viewer.submeshes()[s];

        int begin = sm.beginIndex();
        int count = sm.indexCount();
        if (begin < 0 || count <= 0) return;

        int maxCount = Math.max(0, indexCount - begin);
        if (count > maxCount) count = maxCount;
        count -= (count % 3);
        if (count <= 0) return;

        int rgba = sm.diffuseRGBA();

        float mr = ((rgba >>> 24) & 0xFF) / 255.0f;
        float mg = ((rgba >>> 16) & 0xFF) / 255.0f;
        float mb = ((rgba >>>  8) & 0xFF) / 255.0f;
        float ma = ( rgba         & 0xFF) / 255.0f;
        ma *= sm.alphaMat();

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

        ResourceLocation mainTex = (sm.mainTex() != null) ? sm.mainTex() : viewer.magentaTex();
        RenderSystem.setShaderTexture(0, mainTex);
        RenderSystem.setShaderTexture(1, viewer.magentaTex());
        RenderSystem.setShaderTexture(2, viewer.magentaTex());

        set1i(sh, "u_TexMode", sm.hasMainTex() ? 2 : 0);
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

        if (sm.bothFace()) RenderSystem.disableCull();
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

        RenderSystem.depthMask(true);
        RenderSystem.disableBlend();
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
}
