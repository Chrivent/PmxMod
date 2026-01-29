package net.Chivent.pmxSteveMod.viewer.renderers;

import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.VertexConsumer;
import com.mojang.math.Axis;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.MaterialInfo;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.SubmeshInfo;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.RenderStateShard;
import net.minecraft.client.renderer.LevelRenderer;
import net.minecraft.client.renderer.RenderType;
import net.minecraft.client.renderer.MultiBufferSource;
import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import com.mojang.blaze3d.vertex.VertexFormat;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.platform.GlStateManager;
import com.mojang.logging.LogUtils;
import org.joml.Matrix3f;
import org.joml.Matrix4f;
import org.slf4j.Logger;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.Map;

public class PmxVanillaRenderer extends PmxRenderBase {
    private static final Logger LOGGER = LogUtils.getLogger();
    private static final float TRANSLUCENT_ALPHA_THRESHOLD = 0.999f;
    private static final Map<ResourceLocation, RenderType> TRI_CUTOUT = new HashMap<>();
    private static final Map<ResourceLocation, RenderType> TRI_TRANSLUCENT = new HashMap<>();
    private static final RenderStateShard.TransparencyStateShard PMX_NO_TRANSPARENCY =
            new RenderStateShard.TransparencyStateShard("pmx_no_transparency", RenderSystem::disableBlend, () -> {});
    private static final RenderStateShard.TransparencyStateShard PMX_TRANSLUCENT_TRANSPARENCY =
            new RenderStateShard.TransparencyStateShard("pmx_translucent_transparency", () -> {
                RenderSystem.enableBlend();
                RenderSystem.blendFuncSeparate(
                        GlStateManager.SourceFactor.SRC_ALPHA,
                        GlStateManager.DestFactor.ONE_MINUS_SRC_ALPHA,
                        GlStateManager.SourceFactor.ONE,
                        GlStateManager.DestFactor.ONE_MINUS_SRC_ALPHA
                );
            }, () -> {
                RenderSystem.disableBlend();
                RenderSystem.defaultBlendFunc();
            });
    private static final RenderStateShard.CullStateShard PMX_NO_CULL =
            new RenderStateShard.CullStateShard(false);
    private static final RenderStateShard.LightmapStateShard PMX_LIGHTMAP =
            new RenderStateShard.LightmapStateShard(true);
    private static final RenderStateShard.OverlayStateShard PMX_OVERLAY =
            new RenderStateShard.OverlayStateShard(true);

    public void onViewerShutdown() {
        resetTextureCache();
        TRI_CUTOUT.clear();
        TRI_TRANSLUCENT.clear();
    }

    public void renderPlayer(PmxInstance instance,
                             AbstractClientPlayer player,
                             float partialTick,
                             PoseStack poseStack) {
        if (!instance.isReady() || instance.handle() == 0L) return;
        if (instance.idxBuf() == null || instance.posBuf() == null
                || instance.uvBuf() == null) {
            return;
        }

        instance.syncCpuBuffersForRender();

        poseStack.pushPose();
        float viewYRot = player.getViewYRot(partialTick);
        poseStack.mulPose(Axis.YP.rotationDegrees(-viewYRot));
        poseStack.scale(0.15f, 0.15f, 0.15f);

        Matrix4f pose = poseStack.last().pose();
        Matrix3f normalMat = poseStack.last().normal();

        boolean useNormals = isShaderPackActive() && instance.nrmBuf() != null;
        ByteBuffer posBuf = instance.posBuf().duplicate().order(ByteOrder.nativeOrder());
        ByteBuffer nrmBuf = useNormals ? instance.nrmBuf().duplicate().order(ByteOrder.nativeOrder()) : null;
        ByteBuffer uvBuf = instance.uvBuf().duplicate().order(ByteOrder.nativeOrder());
        ByteBuffer idxBuf = instance.idxBuf().duplicate().order(ByteOrder.nativeOrder());

        int elemSize = net.Chivent.pmxSteveMod.jni.PmxNative.nativeGetIndexElementSize(instance.handle());
        int light = LevelRenderer.getLightColor(player.level(), player.blockPosition());
        MultiBufferSource.BufferSource buffer = Minecraft.getInstance().renderBuffers().bufferSource();
        SubmeshInfo[] subs = instance.submeshes();
        if (subs != null) {
            for (SubmeshInfo sub : subs) {
                MaterialInfo mat = instance.material(sub.materialId());
                if (mat == null) continue;
                float alpha = mat.alpha();
                if (alpha <= 0.0f) continue;

                TextureEntry tex = getOrLoadMainTexture(instance, mat.mainTexPath());
                ResourceLocation rl = tex != null ? tex.rl() : ensureMagentaTexture();
                boolean translucent = alpha < TRANSLUCENT_ALPHA_THRESHOLD;
                RenderType type = rl != null ? getRenderType(rl, translucent) : null;
                VertexConsumer vc = type != null ? buffer.getBuffer(type) : null;

                if (vc == null) continue;
                drawSubmesh(vc, sub, posBuf, nrmBuf, uvBuf, idxBuf, elemSize,
                        pose, normalMat,
                        alpha, light, useNormals);
            }
        }

        buffer.endBatch();
        poseStack.popPose();
    }

    private void drawSubmesh(VertexConsumer vc,
                             SubmeshInfo sub,
                             ByteBuffer posBuf,
                             ByteBuffer nrmBuf,
                             ByteBuffer uvBuf,
                             ByteBuffer idxBuf,
                             int elemSize,
                             Matrix4f pose,
                             Matrix3f normalMat,
                             float alpha,
                             int light,
                             boolean useNormals) {
        int begin = sub.beginIndex();
        int count = sub.indexCount();
        if (begin < 0 || count <= 0) return;
        count -= (count % 3);
        if (count <= 0) return;

        for (int i = 0; i + 2 < count; i += 3) {
            int idx0 = readIndex(idxBuf, elemSize, begin + i);
            int idx1 = readIndex(idxBuf, elemSize, begin + i + 1);
            int idx2 = readIndex(idxBuf, elemSize, begin + i + 2);
            addVertex(vc, pose, normalMat, posBuf, nrmBuf, uvBuf, idx0, alpha, light, useNormals);
            addVertex(vc, pose, normalMat, posBuf, nrmBuf, uvBuf, idx1, alpha, light, useNormals);
            addVertex(vc, pose, normalMat, posBuf, nrmBuf, uvBuf, idx2, alpha, light, useNormals);
        }
    }

    private void addVertex(VertexConsumer vc,
                           Matrix4f pose,
                           Matrix3f normalMat,
                           ByteBuffer posBuf,
                           ByteBuffer nrmBuf,
                           ByteBuffer uvBuf,
                           int index,
                           float a,
                           int light,
                           boolean useNormals) {
        int posBase = index * 12;
        float x = posBuf.getFloat(posBase);
        float y = posBuf.getFloat(posBase + 4);
        float z = posBuf.getFloat(posBase + 8);

        int uvBase = index * 8;
        float u = uvBuf.getFloat(uvBase);
        float v = uvBuf.getFloat(uvBase + 4);

        float nx = 0.0f;
        float ny = 1.0f;
        float nz = 0.0f;
        if (useNormals && nrmBuf != null) {
            int nrmBase = index * 12;
            nx = nrmBuf.getFloat(nrmBase);
            ny = nrmBuf.getFloat(nrmBase + 4);
            nz = nrmBuf.getFloat(nrmBase + 8);
        }

        vc.vertex(pose, x, y, z)
                .color(1.0f, 1.0f, 1.0f, a)
                .uv(u, v)
                .overlayCoords(net.minecraft.client.renderer.texture.OverlayTexture.NO_OVERLAY)
                .uv2(light)
                .normal(normalMat, nx, ny, nz)
                .endVertex();
    }

    private static boolean isShaderPackActive() {
        try {
            Class<?> api = Class.forName("net.irisshaders.iris.api.v0.IrisApi");
            Object inst = api.getMethod("getInstance").invoke(null);
            Object active = api.getMethod("isShaderPackInUse").invoke(inst);
            return active instanceof Boolean b && b;
        } catch (ReflectiveOperationException e) {
            LOGGER.debug("[PMX] Iris/Oculus API not available: {}", "net.irisshaders.iris.api.v0.IrisApi");
            return false;
        } catch (Exception e) {
            LOGGER.debug("[PMX] Iris/Oculus API error: {}", "net.irisshaders.iris.api.v0.IrisApi");
            return false;
        }
    }

    private static int readIndex(ByteBuffer idxBuf, int elemSize, int index) {
        int offset = index * elemSize;
        return switch (elemSize) {
            case 1 -> idxBuf.get(offset) & 0xFF;
            case 2 -> idxBuf.getShort(offset) & 0xFFFF;
            default -> idxBuf.getInt(offset);
        };
    }

    private RenderType getRenderType(ResourceLocation rl, boolean translucent) {
        return translucent
                ? TRI_TRANSLUCENT.computeIfAbsent(rl, PmxVanillaRenderer::createEntityTranslucentTriangles)
                : TRI_CUTOUT.computeIfAbsent(rl, PmxVanillaRenderer::createEntityCutoutNoCullTriangles);
    }

    private static RenderType createEntityCutoutNoCullTriangles(ResourceLocation rl) {
        RenderType.CompositeState state = RenderType.CompositeState.builder()
                .setShaderState(new RenderStateShard.ShaderStateShard(GameRenderer::getRendertypeEntityCutoutNoCullShader))
                .setTextureState(new RenderStateShard.TextureStateShard(rl, false, false))
                .setTransparencyState(PMX_NO_TRANSPARENCY)
                .setCullState(PMX_NO_CULL)
                .setLightmapState(PMX_LIGHTMAP)
                .setOverlayState(PMX_OVERLAY)
                .createCompositeState(true);
        return RenderType.create(
                "pmx_entity_cutout_no_cull_tri",
                DefaultVertexFormat.NEW_ENTITY,
                VertexFormat.Mode.TRIANGLES,
                256,
                true,
                false,
                state
        );
    }

    private static RenderType createEntityTranslucentTriangles(ResourceLocation rl) {
        RenderType.CompositeState state = RenderType.CompositeState.builder()
                .setShaderState(new RenderStateShard.ShaderStateShard(GameRenderer::getRendertypeEntityTranslucentShader))
                .setTextureState(new RenderStateShard.TextureStateShard(rl, false, false))
                .setTransparencyState(PMX_TRANSLUCENT_TRANSPARENCY)
                .setCullState(PMX_NO_CULL)
                .setLightmapState(PMX_LIGHTMAP)
                .setOverlayState(PMX_OVERLAY)
                .createCompositeState(true);
        return RenderType.create(
                "pmx_entity_translucent_tri",
                DefaultVertexFormat.NEW_ENTITY,
                VertexFormat.Mode.TRIANGLES,
                256,
                true,
                true,
                state
        );
    }


    private TextureEntry getOrLoadMainTexture(PmxInstance instance, String texPath) {
        return loadTextureEntryCached(instance, texPath, "pmx/vanilla_tex_", false, false, null);
    }

}
