package net.Chivent.pmxSteveMod.viewer.renderers;

import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.VertexConsumer;
import com.mojang.math.Axis;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.MaterialInfo;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.SubmeshInfo;
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.client.renderer.RenderStateShard;
import net.minecraft.client.renderer.RenderType;
import net.minecraft.client.renderer.GameRenderer;
import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import com.mojang.blaze3d.vertex.VertexFormat;
import net.minecraft.client.renderer.texture.OverlayTexture;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import net.minecraft.Util;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.platform.GlStateManager;
import com.mojang.logging.LogUtils;
import org.joml.Matrix3f;
import org.joml.Matrix4f;
import org.slf4j.Logger;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.function.Function;

public class PmxVanillaRenderer extends PmxRenderBase {
    private static final Logger LOGGER = LogUtils.getLogger();
    private static final float TRANSLUCENT_ALPHA_THRESHOLD = 0.999f;
    private static final long SHADER_PACK_CHECK_INTERVAL_NANOS = 500_000_000L;
    private static boolean cachedShaderPackActive = false;
    private static long lastShaderPackCheckNanos = 0L;
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
    private static final RenderStateShard.ShaderStateShard PMX_CUTOUT_SHADER =
            new RenderStateShard.ShaderStateShard(GameRenderer::getRendertypeEntityCutoutNoCullShader);
    private static final RenderStateShard.ShaderStateShard PMX_TRANSLUCENT_SHADER =
            new RenderStateShard.ShaderStateShard(GameRenderer::getRendertypeEntityTranslucentShader);
    private static final Function<ResourceLocation, RenderType> PMX_ENTITY_CUTOUT = Util.memoize(rl -> {
        RenderType.CompositeState state = RenderType.CompositeState.builder()
                .setShaderState(PMX_CUTOUT_SHADER)
                .setTextureState(new RenderStateShard.TextureStateShard(rl, false, false))
                .setTransparencyState(PMX_NO_TRANSPARENCY)
                .setCullState(PMX_NO_CULL)
                .setLightmapState(PMX_LIGHTMAP)
                .setOverlayState(PMX_OVERLAY)
                .createCompositeState(true);
        return RenderType.create("pmx_entity_cutout", DefaultVertexFormat.NEW_ENTITY,
                VertexFormat.Mode.TRIANGLES, 256, true, false, state);
    });
    private static final Function<ResourceLocation, RenderType> PMX_ENTITY_TRANSLUCENT = Util.memoize(rl -> {
        RenderType.CompositeState state = RenderType.CompositeState.builder()
                .setShaderState(PMX_TRANSLUCENT_SHADER)
                .setTextureState(new RenderStateShard.TextureStateShard(rl, false, false))
                .setTransparencyState(PMX_TRANSLUCENT_TRANSPARENCY)
                .setCullState(PMX_NO_CULL)
                .setLightmapState(PMX_LIGHTMAP)
                .setOverlayState(PMX_OVERLAY)
                .createCompositeState(true);
        return RenderType.create("pmx_entity_translucent", DefaultVertexFormat.NEW_ENTITY,
                VertexFormat.Mode.TRIANGLES, 256, true, true, state);
    });

    public void onViewerShutdown() {
        resetTextureCache();
    }

    public void renderPlayer(PmxInstance instance,
                             AbstractClientPlayer player,
                             float partialTick,
                             PoseStack poseStack,
                             MultiBufferSource buffers,
                             int packedLight) {
        if (!instance.isReady() || instance.handle() == 0L) return;
        if (buffers == null) return;
        if (instance.idxBuf() == null || instance.posBuf() == null
                || instance.uvBuf() == null) {
            return;
        }

        instance.syncCpuBuffersForRender();

        int elemSize = instance.indexElementSize();
        int indexCount = instance.indexCount();
        if (elemSize <= 0 || indexCount <= 0) return;

        ByteBuffer idxBuf = instance.idxBuf().duplicate().order(ByteOrder.nativeOrder());
        FloatBuffer posBuf = toFloatBuffer(instance.posBuf());
        FloatBuffer uvBuf = toFloatBuffer(instance.uvBuf());
        FloatBuffer nrmBuf = instance.nrmBuf() != null ? toFloatBuffer(instance.nrmBuf()) : null;

        int vertexCount = instance.vertexCount();
        if (vertexCount <= 0) {
            vertexCount = posBuf.capacity() / 3;
        }
        if (vertexCount <= 0) return;

        poseStack.pushPose();
        float viewYRot = player.getViewYRot(partialTick);
        poseStack.mulPose(Axis.YP.rotationDegrees(-viewYRot));
        poseStack.scale(0.15f, 0.15f, 0.15f);

        PoseStack.Pose last = poseStack.last();
        Matrix4f pose = last.pose();
        Matrix3f normalMat = last.normal();

        boolean useNormals = isShaderPackActive() && nrmBuf != null;
        int overlay = OverlayTexture.NO_OVERLAY;

        SubmeshInfo[] subs = instance.submeshes();
        if (subs != null) {
            for (SubmeshInfo sub : subs) {
                MaterialInfo mat = instance.material(sub.materialId());
                if (mat == null) continue;
                float alpha = mat.alpha();
                if (alpha <= 0.0f) continue;

                TextureEntry tex = getOrLoadMainTexture(instance, mat.mainTexPath());
                ResourceLocation rl = tex != null ? tex.rl() : ensureMagentaTexture();
                if (rl == null) continue;

                boolean translucent = alpha < TRANSLUCENT_ALPHA_THRESHOLD;
                RenderType type = translucent
                        ? PMX_ENTITY_TRANSLUCENT.apply(rl)
                        : PMX_ENTITY_CUTOUT.apply(rl);
                VertexConsumer vc = buffers.getBuffer(type);
                emitSubmesh(vc, sub, idxBuf, elemSize, indexCount,
                        posBuf, uvBuf, nrmBuf, vertexCount,
                        pose, normalMat, alpha, packedLight, overlay, useNormals);
            }
        }
        poseStack.popPose();
    }

    private static boolean isShaderPackActive() {
        long now = System.nanoTime();
        if (now - lastShaderPackCheckNanos < SHADER_PACK_CHECK_INTERVAL_NANOS) {
            return cachedShaderPackActive;
        }
        lastShaderPackCheckNanos = now;
        boolean next = false;
        try {
            Class<?> api = Class.forName("net.irisshaders.iris.api.v0.IrisApi");
            Object inst = api.getMethod("getInstance").invoke(null);
            Object active = api.getMethod("isShaderPackInUse").invoke(inst);
            next = active instanceof Boolean b && b;
        } catch (ReflectiveOperationException e) {
            LOGGER.debug("[PMX] Iris/Oculus API not available: {}", "net.irisshaders.iris.api.v0.IrisApi");
        } catch (Exception e) {
            LOGGER.debug("[PMX] Iris/Oculus API error: {}", "net.irisshaders.iris.api.v0.IrisApi");
        }
        if (next != cachedShaderPackActive) {
            LOGGER.debug("[PMX] Shader pack active changed: {}", next);
        }
        cachedShaderPackActive = next;
        return cachedShaderPackActive;
    }

    private TextureEntry getOrLoadMainTexture(PmxInstance instance, String texPath) {
        return loadTextureEntryCached(instance, texPath, "pmx/vanilla_tex_", false, false, null);
    }

    private static void emitSubmesh(VertexConsumer vc,
                                    SubmeshInfo sub,
                                    ByteBuffer idxBuf,
                                    int elemSize,
                                    int indexCount,
                                    FloatBuffer posBuf,
                                    FloatBuffer uvBuf,
                                    FloatBuffer nrmBuf,
                                    int vertexCount,
                                    Matrix4f pose,
                                    Matrix3f normalMat,
                                    float alpha,
                                    int packedLight,
                                    int overlay,
                                    boolean useNormals) {
        DrawRange range = getDrawRange(sub, indexCount, elemSize);
        if (range == null) return;
        int begin = sub.beginIndex();
        int count = range.count();
        float nxConst = 0.0f;
        float nyConst = 1.0f;
        float nzConst = 0.0f;
        for (int i = 0; i < count; i++) {
            int idx = readIndex(idxBuf, elemSize, begin + i);
            if (idx < 0 || idx >= vertexCount) continue;
            int posBase = idx * 3;
            int uvBase = idx * 2;
            float x = posBuf.get(posBase);
            float y = posBuf.get(posBase + 1);
            float z = posBuf.get(posBase + 2);
            float u = uvBuf.get(uvBase);
            float v = uvBuf.get(uvBase + 1);
            float nx = nxConst;
            float ny = nyConst;
            float nz = nzConst;
            if (useNormals && nrmBuf != null) {
                int nrmBase = idx * 3;
                nx = nrmBuf.get(nrmBase);
                ny = nrmBuf.get(nrmBase + 1);
                nz = nrmBuf.get(nrmBase + 2);
            }
            vc.vertex(pose, x, y, z)
                    .color(1.0f, 1.0f, 1.0f, alpha)
                    .uv(u, v)
                    .overlayCoords(overlay)
                    .uv2(packedLight)
                    .normal(normalMat, nx, ny, nz)
                    .endVertex();
        }
    }

    private static int readIndex(ByteBuffer idxBuf, int elemSize, int ordinal) {
        int offset = ordinal * elemSize;
        return switch (elemSize) {
            case 1 -> idxBuf.get(offset) & 0xFF;
            case 2 -> idxBuf.getShort(offset) & 0xFFFF;
            default -> idxBuf.getInt(offset);
        };
    }

    private static FloatBuffer toFloatBuffer(ByteBuffer buffer) {
        ByteBuffer dup = buffer.duplicate().order(ByteOrder.nativeOrder());
        dup.rewind();
        return dup.asFloatBuffer();
    }
}
