package net.Chivent.pmxSteveMod.viewer.renderers;

import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.math.Axis;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.MaterialInfo;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.SubmeshInfo;
import net.minecraft.client.renderer.RenderType;
import net.minecraft.client.renderer.ShaderInstance;
import net.minecraft.client.renderer.texture.OverlayTexture;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import com.mojang.blaze3d.systems.RenderSystem;
import org.joml.Matrix4f;
import org.joml.Vector3f;
import org.lwjgl.opengl.GL11C;
import org.lwjgl.opengl.GL20C;
import org.lwjgl.opengl.GL30C;
import net.Chivent.pmxSteveMod.client.util.PmxShaderPackUtil;

public class PmxOculusRenderer extends PmxRenderBase {
    private static final float TRANSLUCENT_ALPHA_THRESHOLD = 0.999f;

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
        float bodyYaw = getBodyYaw(player, partialTick);
        poseStack.mulPose(Axis.YP.rotationDegrees(-bodyYaw));
        applyVanillaBodyTilt(player, partialTick, poseStack);
        poseStack.scale(MODEL_SCALE, MODEL_SCALE, MODEL_SCALE);

        PoseStack.Pose last = poseStack.last();
        Matrix4f pose = last.pose();
        boolean useNormals = PmxShaderPackUtil.isShaderPackActive() && instance.nrmBuf() != null;
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
                        ? RenderType.entityTranslucent(rl)
                        : RenderType.entityCutoutNoCull(rl);
                drawSubmeshIndexed(sub, type, alpha, packedLight, overlay, useNormals, pose);
            }
        }

        poseStack.popPose();
    }

    // Shader pack detection handled by PmxShaderPackUtil.

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
        Vector3f fixedLight = !useNormals ? new Vector3f(0.0f, 1.0f, 0.0f) : null;
        if (shader != null) {
            applyShaderUniforms(shader, pose, alpha, fixedLight);
        }

        GL30C.glBindVertexArray(mesh.vao);
        setConstantAttributes(alpha, packedLight, overlay, useNormals);
        GL11C.glDrawElements(GL11C.GL_TRIANGLES, range.count(), mesh.glIndexType, range.offsetBytes());
        GL30C.glBindVertexArray(0);

        renderType.clearRenderState();
    }

    private static void applyShaderUniforms(ShaderInstance shader,
                                            Matrix4f modelView,
                                            float alpha,
                                            Vector3f fixedLight) {
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
        if (fixedLight != null) {
            if (shader.LIGHT0_DIRECTION != null) {
                shader.LIGHT0_DIRECTION.set(fixedLight);
            }
            if (shader.LIGHT1_DIRECTION != null) {
                shader.LIGHT1_DIRECTION.set(fixedLight);
            }
        }
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
