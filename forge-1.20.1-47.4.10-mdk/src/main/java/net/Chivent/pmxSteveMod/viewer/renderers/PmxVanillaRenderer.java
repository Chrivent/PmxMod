package net.Chivent.pmxSteveMod.viewer.renderers;

import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.VertexFormat;
import com.mojang.logging.LogUtils;
import com.mojang.math.Axis;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.MaterialInfo;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.SubmeshInfo;
import net.minecraft.Util;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.renderer.RenderStateShard;
import net.minecraft.client.renderer.RenderType;
import net.minecraft.client.renderer.ShaderInstance;
import net.minecraft.client.renderer.texture.OverlayTexture;
import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.platform.GlStateManager;
import org.joml.Matrix4f;
import org.lwjgl.opengl.GL11C;
import org.lwjgl.opengl.GL20C;
import org.lwjgl.opengl.GL30C;
import org.slf4j.Logger;

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

    private final PmxGlMesh mesh = new PmxGlMesh();

    public void onViewerShutdown() {
        resetTextureCache();
        mesh.destroy();
    }

    public void renderPlayer(PmxInstance instance,
                             AbstractClientPlayer player,
                             float partialTick,
                             PoseStack poseStack,
                             int packedLight) {
        if (!instance.isReady() || instance.handle() == 0L) return;
        if (instance.idxBuf() == null || instance.posBuf() == null
                || instance.uvBuf() == null) {
            return;
        }

        if (shouldSkipMeshUpdate(instance, mesh)) return;

        poseStack.pushPose();
        float viewYRot = player.getViewYRot(partialTick);
        poseStack.mulPose(Axis.YP.rotationDegrees(-viewYRot));
        poseStack.scale(0.15f, 0.15f, 0.15f);

        PoseStack.Pose last = poseStack.last();
        Matrix4f pose = last.pose();
        boolean useNormals = isShaderPackActive() && instance.nrmBuf() != null;
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
                drawSubmeshIndexed(sub, type, alpha, packedLight, overlay, useNormals, pose);
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

    private void drawSubmeshIndexed(SubmeshInfo sub,
                                    RenderType renderType,
                                    float alpha,
                                    int packedLight,
                                    int overlay,
                                    boolean useNormals,
                                    Matrix4f pose) {
        DrawRange range = getDrawRange(sub, mesh);
        if (range == null) return;
        renderType.setupRenderState();
        ShaderInstance shader = RenderSystem.getShader();
        if (shader != null) {
            applyShaderUniforms(shader, pose, alpha);
        }

        GL30C.glBindVertexArray(mesh.vao);
        setConstantAttributes(alpha, packedLight, overlay, useNormals);
        GL11C.glDrawElements(GL11C.GL_TRIANGLES, range.count(), mesh.glIndexType, range.offsetBytes());
        GL30C.glBindVertexArray(0);

        renderType.clearRenderState();
    }

    private static void applyShaderUniforms(ShaderInstance shader,
                                            Matrix4f modelView,
                                            float alpha) {
        for (int i = 0; i < 12; i++) {
            int texId = RenderSystem.getShaderTexture(i);
            shader.setSampler("Sampler" + i, texId);
        }
        if (shader.MODEL_VIEW_MATRIX != null) {
            shader.MODEL_VIEW_MATRIX.set(modelView);
        }
        if (shader.PROJECTION_MATRIX != null) {
            shader.PROJECTION_MATRIX.set(RenderSystem.getProjectionMatrix());
        }
        if (shader.INVERSE_VIEW_ROTATION_MATRIX != null) {
            shader.INVERSE_VIEW_ROTATION_MATRIX.set(RenderSystem.getInverseViewRotationMatrix());
        }
        if (shader.COLOR_MODULATOR != null) {
            shader.COLOR_MODULATOR.set(1.0f, 1.0f, 1.0f, alpha);
        }
        if (shader.GLINT_ALPHA != null) {
            shader.GLINT_ALPHA.set(RenderSystem.getShaderGlintAlpha());
        }
        if (shader.FOG_START != null) {
            shader.FOG_START.set(RenderSystem.getShaderFogStart());
        }
        if (shader.FOG_END != null) {
            shader.FOG_END.set(RenderSystem.getShaderFogEnd());
        }
        if (shader.FOG_COLOR != null) {
            shader.FOG_COLOR.set(RenderSystem.getShaderFogColor());
        }
        if (shader.FOG_SHAPE != null) {
            shader.FOG_SHAPE.set(RenderSystem.getShaderFogShape().getIndex());
        }
        if (shader.TEXTURE_MATRIX != null) {
            shader.TEXTURE_MATRIX.set(RenderSystem.getTextureMatrix());
        }
        if (shader.GAME_TIME != null) {
            shader.GAME_TIME.set(RenderSystem.getShaderGameTime());
        }
        if (shader.SCREEN_SIZE != null) {
            var window = net.minecraft.client.Minecraft.getInstance().getWindow();
            shader.SCREEN_SIZE.set((float) window.getWidth(), (float) window.getHeight());
        }
        RenderSystem.setupShaderLights(shader);
        shader.apply();
    }

    private static void setConstantAttributes(float alpha, int packedLight, int overlay, boolean useNormals) {
        GL20C.glDisableVertexAttribArray(PmxGlMesh.LOC_COLOR);
        GL20C.glDisableVertexAttribArray(PmxGlMesh.LOC_UV1);
        GL20C.glDisableVertexAttribArray(PmxGlMesh.LOC_UV2);

        GL20C.glVertexAttrib4f(PmxGlMesh.LOC_COLOR, 1.0f, 1.0f, 1.0f, alpha);
        int overlayU = (overlay & 0xFFFF);
        int overlayV = (overlay >>> 16);
        GL30C.glVertexAttribI2i(PmxGlMesh.LOC_UV1, overlayU, overlayV);
        int lightU = (packedLight & 0xFFFF);
        int lightV = (packedLight >>> 16);
        GL30C.glVertexAttribI2i(PmxGlMesh.LOC_UV2, lightU, lightV);

        if (!useNormals) {
            GL20C.glDisableVertexAttribArray(PmxGlMesh.LOC_NRM);
            GL20C.glVertexAttrib3f(PmxGlMesh.LOC_NRM, 0.0f, 1.0f, 0.0f);
        } else {
            GL20C.glEnableVertexAttribArray(PmxGlMesh.LOC_NRM);
        }
    }
}
