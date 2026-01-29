package net.Chivent.pmxSteveMod.viewer.renderers;

import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.math.Axis;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.MaterialInfo;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.SubmeshInfo;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.renderer.RenderStateShard;
import net.minecraft.client.renderer.LevelRenderer;
import net.minecraft.client.renderer.ShaderInstance;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.platform.GlStateManager;
import com.mojang.logging.LogUtils;
import org.joml.Matrix4f;
import org.slf4j.Logger;
import org.lwjgl.opengl.GL11C;
import org.lwjgl.opengl.GL20C;
import org.lwjgl.opengl.GL30C;

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
    private final PmxGlMesh mesh = new PmxGlMesh();

    public void onViewerShutdown() {
        resetTextureCache();
        mesh.destroy();
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

        boolean useNormals = isShaderPackActive() && instance.nrmBuf() != null;
        int light = LevelRenderer.getLightColor(player.level(), player.blockPosition());
        mesh.ensure(instance);
        if (!mesh.ready) {
            poseStack.popPose();
            return;
        }
        mesh.updateDynamic(instance);
        SubmeshInfo[] subs = instance.submeshes();
        if (subs != null) {
            RenderSystem.getModelViewStack().pushPose();
            RenderSystem.getModelViewStack().mulPoseMatrix(pose);
            RenderSystem.applyModelViewMatrix();
            for (SubmeshInfo sub : subs) {
                MaterialInfo mat = instance.material(sub.materialId());
                if (mat == null) continue;
                float alpha = mat.alpha();
                if (alpha <= 0.0f) continue;

                TextureEntry tex = getOrLoadMainTexture(instance, mat.mainTexPath());
                ResourceLocation rl = tex != null ? tex.rl() : ensureMagentaTexture();
                boolean translucent = alpha < TRANSLUCENT_ALPHA_THRESHOLD;
                if (rl == null) continue;
                drawSubmeshIndexed(sub, rl, translucent, alpha, light, useNormals, pose);
            }
            RenderSystem.getModelViewStack().popPose();
            RenderSystem.applyModelViewMatrix();
        }
        poseStack.popPose();
    }

    private void drawSubmeshIndexed(SubmeshInfo sub,
                                    ResourceLocation rl,
                                    boolean translucent,
                                    float alpha,
                                    int light,
                                    boolean useNormals,
                                    Matrix4f pose) {
        int begin = sub.beginIndex();
        int count = sub.indexCount();
        if (begin < 0 || count <= 0) return;
        count -= (count % 3);
        if (count <= 0) return;
        setupRenderState(translucent);
        ShaderInstance shader = bindShader(rl, translucent);
        if (shader != null) {
            applyShaderUniforms(shader, pose, rl);
        }

        GL30C.glBindVertexArray(mesh.vao);

        setConstantAttributes(alpha, light, useNormals);

        long offsetBytes = (long) begin * (long) mesh.elemSize;
        GL11C.glDrawElements(GL11C.GL_TRIANGLES, count, mesh.glIndexType, offsetBytes);

        GL30C.glBindVertexArray(0);

        clearRenderState(translucent);
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

    private static void setConstantAttributes(float alpha, int light, boolean useNormals) {
        GL20C.glDisableVertexAttribArray(PmxGlMesh.LOC_COLOR);
        GL20C.glDisableVertexAttribArray(PmxGlMesh.LOC_UV1);
        GL20C.glDisableVertexAttribArray(PmxGlMesh.LOC_UV2);

        GL20C.glVertexAttrib4f(PmxGlMesh.LOC_COLOR, 1.0f, 1.0f, 1.0f, alpha);
        int overlay = net.minecraft.client.renderer.texture.OverlayTexture.NO_OVERLAY;
        int overlayU = (overlay & 0xFFFF);
        int overlayV = (overlay >>> 16);
        GL30C.glVertexAttribI2i(PmxGlMesh.LOC_UV1, overlayU, overlayV);
        int lightU = (light & 0xFFFF);
        int lightV = (light >>> 16);
        GL30C.glVertexAttribI2i(PmxGlMesh.LOC_UV2, lightU, lightV);

        if (!useNormals) {
            GL20C.glDisableVertexAttribArray(PmxGlMesh.LOC_NRM);
            GL20C.glVertexAttrib3f(PmxGlMesh.LOC_NRM, 0.0f, 1.0f, 0.0f);
        } else {
            GL20C.glEnableVertexAttribArray(PmxGlMesh.LOC_NRM);
        }
    }

    private static ShaderInstance bindShader(ResourceLocation rl, boolean translucent) {
        RenderSystem.setShader(translucent
                ? GameRenderer::getRendertypeEntityTranslucentShader
                : GameRenderer::getRendertypeEntityCutoutNoCullShader);
        RenderSystem.setShaderTexture(0, rl);
        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);
        ShaderInstance shader = RenderSystem.getShader();
        return shader;
    }

    private static void applyShaderUniforms(ShaderInstance shader, Matrix4f modelView, ResourceLocation rl) {
        var texMgr = Minecraft.getInstance().getTextureManager();
        shader.setSampler("Sampler0", texMgr.getTexture(rl));
        for (int i = 1; i < 12; i++) {
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
            shader.COLOR_MODULATOR.set(RenderSystem.getShaderColor());
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
            var window = Minecraft.getInstance().getWindow();
            shader.SCREEN_SIZE.set((float) window.getWidth(), (float) window.getHeight());
        }
        RenderSystem.setupShaderLights(shader);
        shader.apply();
    }

    private static void setupRenderState(boolean translucent) {
        if (translucent) {
            PMX_TRANSLUCENT_TRANSPARENCY.setupRenderState();
        } else {
            PMX_NO_TRANSPARENCY.setupRenderState();
        }
        PMX_LIGHTMAP.setupRenderState();
        PMX_OVERLAY.setupRenderState();
        PMX_NO_CULL.setupRenderState();
    }

    private static void clearRenderState(boolean translucent) {
        PMX_NO_CULL.clearRenderState();
        PMX_OVERLAY.clearRenderState();
        PMX_LIGHTMAP.clearRenderState();
        if (translucent) {
            PMX_TRANSLUCENT_TRANSPARENCY.clearRenderState();
        } else {
            PMX_NO_TRANSPARENCY.clearRenderState();
        }
    }

}
