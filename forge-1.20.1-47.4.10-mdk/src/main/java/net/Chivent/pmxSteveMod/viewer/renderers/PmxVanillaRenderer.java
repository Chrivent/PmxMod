package net.Chivent.pmxSteveMod.viewer.renderers;

import com.mojang.blaze3d.shaders.Uniform;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.math.Axis;
import com.mojang.logging.LogUtils;
import net.Chivent.pmxSteveMod.client.PmxShaders;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.MaterialInfo;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.SubmeshInfo;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.ShaderInstance;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.client.renderer.texture.TextureManager;
import net.minecraft.resources.ResourceLocation;
import net.minecraft.world.level.Level;
import org.joml.Matrix3f;
import org.joml.Matrix4f;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.GL11C;
import org.lwjgl.opengl.GL30C;
import org.slf4j.Logger;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class PmxVanillaRenderer extends PmxRenderBase {

    private static final Logger LOGGER = LogUtils.getLogger();
    private static final int MSAA_SAMPLES = 4;
    private final Set<String> textureLoadFailures = new HashSet<>();

    private static final class MaterialGpu {
        int texMode;
        int toonMode;
        int sphereMode;

        ResourceLocation mainTex;
        ResourceLocation sphereTex;
        ResourceLocation toonTex;
    }

    private final Map<Integer, MaterialGpu> materialGpuCache = new HashMap<>();
    private long cachedModelVersion = -1L;

    private final PmxGlMesh mesh = new PmxGlMesh();
    private int msaaFbo = 0;
    private int msaaColorRb = 0;
    private int msaaDepthRb = 0;
    private int msaaWidth = 0;
    private int msaaHeight = 0;
    private int msaaSourceFbo = 0;
    private int msaaColorFormat = GL30C.GL_RGBA8;
    private int msaaDepthFormat = GL30C.GL_DEPTH24_STENCIL8;

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
    private void set2f(ShaderInstance sh, String name, float x, float y) {
        Uniform u = sh.getUniform(name);
        if (u != null) u.set(x, y);
    }
    private void set1i(ShaderInstance sh, String name, int v) {
        Uniform u = sh.getUniform(name);
        if (u != null) u.set(v);
    }

    public void onViewerShutdown() {
        RenderSystem.recordRenderCall(() -> {
            mesh.destroy();
            destroyMsaaTargets();
            resetTextureCache();
            materialGpuCache.clear();
            textureLoadFailures.clear();
            cachedModelVersion = -1L;
        });
    }

    public void renderPlayer(PmxInstance instance,
                             AbstractClientPlayer player,
                             float partialTick,
                             PoseStack poseStack,
                             int packedLight) {
        if (!instance.isReady() || instance.handle() == 0L
                || instance.idxBuf() == null || instance.posBuf() == null
                || instance.nrmBuf() == null || instance.uvBuf() == null) {
            return;
        }

        ShaderInstance sh = PmxShaders.PMX_MMD;
        if (sh == null) return;

        long version = instance.modelVersion();
        if (cachedModelVersion != version) {
            materialGpuCache.clear();
            resetTextureCache();
            textureLoadFailures.clear();
            cachedModelVersion = version;
        }

        if (shouldSkipMeshUpdate(instance, mesh)) return;

        poseStack.pushPose();

        float viewYRot = player.getViewYRot(partialTick);
        poseStack.mulPose(Axis.YP.rotationDegrees(-viewYRot));
        applyVanillaBodyTilt(player, partialTick, poseStack);
        poseStack.scale(MODEL_SCALE, MODEL_SCALE, MODEL_SCALE);

        Matrix4f pose = poseStack.last().pose();
        float[] lightDir = getSunLightDir(player.level(), partialTick);
        float[] lightUV = getLightmapUV(packedLight);

        SubmeshInfo[] subs = instance.submeshes();
        if (subs == null) {
            poseStack.popPose();
            return;
        }

        var window = Minecraft.getInstance().getWindow();
        int width = window.getWidth();
        int height = window.getHeight();
        int prevFbo = beginMsaa(width, height);
        boolean useMsaa = prevFbo >= 0;

        RenderSystem.enableDepthTest();
        RenderSystem.enableBlend();
        RenderSystem.defaultBlendFunc();
        RenderSystem.depthMask(true);
        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

        GL30C.glBindVertexArray(mesh.vao);

        for (SubmeshInfo sub : subs) {
            drawSubmeshIndexed(instance, sub, pose, lightDir, lightUV);
        }

        drawEdgePass(instance, subs, pose);
        GL30C.glBindVertexArray(0);

        RenderSystem.depthMask(true);
        RenderSystem.disableBlend();
        RenderSystem.enableCull();

        if (useMsaa) {
            endMsaa(prevFbo, width, height);
        }

        poseStack.popPose();
    }

    private void drawSubmeshIndexed(PmxInstance instance,
                                    SubmeshInfo sm,
                                    Matrix4f pose,
                                    float[] lightDir,
                                    float[] lightUV) {
        if (sm == null) return;

        MaterialInfo mat = instance.material(sm.materialId());
        if (mat == null) return;

        DrawRange range = getDrawRange(sm, mesh);
        if (range == null) return;

        float alpha = mat.alpha();
        if (alpha <= 0.0f) return;

        MaterialGpu gpu = getOrBuildMaterialGpu(instance, sm.materialId(), mat);

        ShaderInstance sh = PmxShaders.PMX_MMD;
        if (sh == null) return;

        Matrix4f wv = new Matrix4f(pose);
        Matrix4f wvp = new Matrix4f(RenderSystem.getProjectionMatrix()).mul(pose);

        RenderSystem.setShader(() -> sh);

        setMat4(sh, "u_WV", wv);
        setMat4(sh, "u_WVP", wvp);

        set3f(sh, "u_LightColor", 1f, 1f, 1f);
        set3f(sh, "u_LightDir", lightDir[0], lightDir[1], lightDir[2]);
        set2f(sh, "u_LightUV", lightUV[0], lightUV[1]);

        set3f(sh, "u_Ambient", mat.ambientR(), mat.ambientG(), mat.ambientB());

        int rgba = mat.diffuseRGBA();
        float mr = ((rgba >>> 24) & 0xFF) / 255.0f;
        float mg = ((rgba >>> 16) & 0xFF) / 255.0f;
        float mb = ((rgba >>>  8) & 0xFF) / 255.0f;
        set3f(sh, "u_Diffuse", mr, mg, mb);

        set3f(sh, "u_Specular", mat.specularR(), mat.specularG(), mat.specularB());
        set1f(sh, "u_SpecularPower", mat.specularPower());

        set1f(sh, "u_Alpha", alpha);

        TextureManager texMgr = Minecraft.getInstance().getTextureManager();
        sh.setSampler("Sampler0", texMgr.getTexture(gpu.mainTex));
        sh.setSampler("Sampler1", texMgr.getTexture(gpu.toonTex));
        sh.setSampler("Sampler2", texMgr.getTexture(gpu.sphereTex));
        Minecraft.getInstance().gameRenderer.lightTexture().turnOnLightLayer();
        sh.setSampler("Sampler3", RenderSystem.getShaderTexture(2));

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

        sh.apply();

        RenderSystem.enableDepthTest();
        RenderSystem.enableBlend();
        RenderSystem.defaultBlendFunc();
        RenderSystem.depthMask(true);

        if (mat.bothFace()) RenderSystem.disableCull();
        else RenderSystem.enableCull();

        GL11C.glDrawElements(GL11C.GL_TRIANGLES, range.count(), mesh.glIndexType, range.offsetBytes());

        RenderSystem.enableCull();
    }

    private void drawEdgePass(PmxInstance instance, SubmeshInfo[] subs, Matrix4f pose) {
        ShaderInstance edgeShader = PmxShaders.PMX_EDGE;
        if (edgeShader == null || subs == null) return;

        Matrix4f wv = new Matrix4f(pose);
        Matrix4f wvp = new Matrix4f(RenderSystem.getProjectionMatrix()).mul(pose);

        RenderSystem.setShader(() -> edgeShader);
        setMat4(edgeShader, "u_WV", wv);
        setMat4(edgeShader, "u_WVP", wvp);

        var window = Minecraft.getInstance().getWindow();
        set2f(edgeShader, "u_ScreenSize", window.getWidth(), window.getHeight());

        RenderSystem.enableDepthTest();
        RenderSystem.enableBlend();
        RenderSystem.defaultBlendFunc();
        RenderSystem.depthMask(true);

        RenderSystem.enableCull();
        GL11C.glCullFace(GL11C.GL_FRONT);

        for (SubmeshInfo sub : subs) {
            MaterialInfo mat = instance.material(sub.materialId());
            if (mat == null || !mat.edgeFlag()) continue;
            if (mat.alpha() <= 0.0f) continue;

            float edgeSize = mat.edgeSize();
            float[] edgeColor = mat.edgeColor();
            set1f(edgeShader, "u_EdgeSize", edgeSize);
            set4f(edgeShader, "u_EdgeColor", edgeColor[0], edgeColor[1], edgeColor[2], edgeColor[3]);
            edgeShader.apply();

            DrawRange range = getDrawRange(sub, mesh);
            if (range == null) continue;

            GL11C.glDrawElements(GL11C.GL_TRIANGLES, range.count(), mesh.glIndexType, range.offsetBytes());
        }

        GL11C.glCullFace(GL11C.GL_BACK);
    }

    private static float[] getSunLightDir(Level level, float partialTick) {
        if (level == null) return new float[] {0.2f, 1.0f, 0.2f};
        float time = level.getTimeOfDay(partialTick);
        float angleDeg = time * 360.0f;
        Quaternionf rot = new Quaternionf()
                .rotateY((float) Math.toRadians(-90.0f))
                .rotateX((float) Math.toRadians(angleDeg));
        Vector3f dir = new Vector3f(0.0f, 1.0f, 0.0f);
        rot.transform(dir);
        if (dir.lengthSquared() < 1.0e-4f) return new float[] {0.2f, 1.0f, 0.2f};
        dir.normalize();
        float timeOfDay = level.getTimeOfDay(partialTick);
        boolean isDay = timeOfDay >= 0.25f && timeOfDay <= 0.75f;
        if (!isDay) {
            dir.negate();
        }
        Matrix3f invView = RenderSystem.getInverseViewRotationMatrix();
        Matrix3f viewRot = new Matrix3f(invView).invert();
        viewRot.transform(dir);
        return new float[] {dir.x, dir.y, dir.z};
    }

    private static float[] getLightmapUV(int packedLight) {
        int lightU = (packedLight & 0xFFFF);
        int lightV = (packedLight >>> 16);
        float u = (lightU / 16.0f + 0.5f) / 16.0f;
        float v = (lightV / 16.0f + 0.5f) / 16.0f;
        return new float[] {u, v};
    }

    private int beginMsaa(int width, int height) {
        if (width <= 0 || height <= 0) return -1;
        int prevFbo = GL11C.glGetInteger(GL30C.GL_FRAMEBUFFER_BINDING);
        if (!ensureMsaaTargets(prevFbo, width, height)) return -1;

        GL30C.glBindFramebuffer(GL30C.GL_DRAW_FRAMEBUFFER, msaaFbo);
        GL30C.glBindFramebuffer(GL30C.GL_READ_FRAMEBUFFER, prevFbo);
        GL30C.glBlitFramebuffer(
                0, 0, width, height,
                0, 0, width, height,
                GL11C.GL_COLOR_BUFFER_BIT | GL11C.GL_DEPTH_BUFFER_BIT,
                GL11C.GL_NEAREST
        );
        GL30C.glBindFramebuffer(GL30C.GL_FRAMEBUFFER, msaaFbo);
        GL11C.glDrawBuffer(GL30C.GL_COLOR_ATTACHMENT0);
        return prevFbo;
    }

    private void endMsaa(int prevFbo, int width, int height) {
        if (msaaFbo == 0 || prevFbo < 0) return;
        GL30C.glBindFramebuffer(GL30C.GL_READ_FRAMEBUFFER, msaaFbo);
        GL30C.glBindFramebuffer(GL30C.GL_DRAW_FRAMEBUFFER, prevFbo);
        GL30C.glBlitFramebuffer(
                0, 0, width, height,
                0, 0, width, height,
                GL11C.GL_COLOR_BUFFER_BIT | GL11C.GL_DEPTH_BUFFER_BIT,
                GL11C.GL_NEAREST
        );
        GL30C.glBindFramebuffer(GL30C.GL_FRAMEBUFFER, prevFbo);
    }

    private void destroyMsaaTargets() {
        if (msaaColorRb != 0) GL30C.glDeleteRenderbuffers(msaaColorRb);
        if (msaaDepthRb != 0) GL30C.glDeleteRenderbuffers(msaaDepthRb);
        if (msaaFbo != 0) GL30C.glDeleteFramebuffers(msaaFbo);
        msaaColorRb = 0;
        msaaDepthRb = 0;
        msaaFbo = 0;
        msaaWidth = 0;
        msaaHeight = 0;
        msaaSourceFbo = 0;
    }

    private boolean ensureMsaaTargets(int prevFbo, int width, int height) {
        if (msaaFbo != 0
                && msaaSourceFbo == prevFbo
                && msaaWidth == width
                && msaaHeight == height) {
            return true;
        }

        int colorFormat = queryColorInternalFormat(prevFbo);
        int depthFormat = queryDepthInternalFormat(prevFbo);
        if (msaaFbo != 0
                && msaaWidth == width
                && msaaHeight == height
                && msaaColorFormat == colorFormat
                && msaaDepthFormat == depthFormat) {
            msaaSourceFbo = prevFbo;
            return true;
        }

        destroyMsaaTargets();
        msaaWidth = width;
        msaaHeight = height;
        msaaSourceFbo = prevFbo;
        msaaColorFormat = colorFormat;
        msaaDepthFormat = depthFormat;

        msaaFbo = GL30C.glGenFramebuffers();
        GL30C.glBindFramebuffer(GL30C.GL_FRAMEBUFFER, msaaFbo);

        msaaColorRb = GL30C.glGenRenderbuffers();
        GL30C.glBindRenderbuffer(GL30C.GL_RENDERBUFFER, msaaColorRb);
        GL30C.glRenderbufferStorageMultisample(
                GL30C.GL_RENDERBUFFER, MSAA_SAMPLES, msaaColorFormat, width, height
        );
        GL30C.glFramebufferRenderbuffer(
                GL30C.GL_FRAMEBUFFER, GL30C.GL_COLOR_ATTACHMENT0, GL30C.GL_RENDERBUFFER, msaaColorRb
        );

        msaaDepthRb = GL30C.glGenRenderbuffers();
        GL30C.glBindRenderbuffer(GL30C.GL_RENDERBUFFER, msaaDepthRb);
        GL30C.glRenderbufferStorageMultisample(
                GL30C.GL_RENDERBUFFER, MSAA_SAMPLES, msaaDepthFormat, width, height
        );
        GL30C.glFramebufferRenderbuffer(
                GL30C.GL_FRAMEBUFFER, GL30C.GL_DEPTH_STENCIL_ATTACHMENT, GL30C.GL_RENDERBUFFER, msaaDepthRb
        );

        GL11C.glDrawBuffer(GL30C.GL_COLOR_ATTACHMENT0);
        GL11C.glReadBuffer(GL30C.GL_COLOR_ATTACHMENT0);
        int status = GL30C.glCheckFramebufferStatus(GL30C.GL_FRAMEBUFFER);
        GL30C.glBindFramebuffer(GL30C.GL_FRAMEBUFFER, 0);
        return status == GL30C.GL_FRAMEBUFFER_COMPLETE;
    }

    private int queryColorInternalFormat(int fbo) {
        if (fbo == 0) return GL30C.GL_RGBA8;
        int type = getFramebufferAttachmentInt(fbo, GL30C.GL_COLOR_ATTACHMENT0, GL30C.GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
        if (type == GL11C.GL_NONE) return GL30C.GL_RGBA8;
        int name = getFramebufferAttachmentInt(fbo, GL30C.GL_COLOR_ATTACHMENT0, GL30C.GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
        return getAttachmentInternalFormat(type, name);
    }

    private int queryDepthInternalFormat(int fbo) {
        if (fbo == 0) return GL30C.GL_DEPTH24_STENCIL8;
        int attachment = GL30C.GL_DEPTH_STENCIL_ATTACHMENT;
        int type = getFramebufferAttachmentInt(fbo, attachment, GL30C.GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
        if (type == GL11C.GL_NONE) {
            attachment = GL30C.GL_DEPTH_ATTACHMENT;
            type = getFramebufferAttachmentInt(fbo, attachment, GL30C.GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
        }
        if (type == GL11C.GL_NONE) return GL30C.GL_DEPTH24_STENCIL8;
        int name = getFramebufferAttachmentInt(fbo, attachment, GL30C.GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
        int fmt = getAttachmentInternalFormat(type, name);
        return fmt != 0 ? fmt : GL30C.GL_DEPTH24_STENCIL8;
    }

    private static int getFramebufferAttachmentInt(int fbo, int attachment, int pname) {
        int prevFbo = GL11C.glGetInteger(GL30C.GL_FRAMEBUFFER_BINDING);
        var buf = BufferUtils.createIntBuffer(1);
        GL30C.glBindFramebuffer(GL30C.GL_FRAMEBUFFER, fbo);
        GL30C.glGetFramebufferAttachmentParameteriv(
                GL30C.GL_FRAMEBUFFER, attachment, pname, buf
        );
        GL30C.glBindFramebuffer(GL30C.GL_FRAMEBUFFER, prevFbo);
        return buf.get(0);
    }

    private static int getAttachmentInternalFormat(int objectType, int name) {
        var buf = BufferUtils.createIntBuffer(1);
        if (objectType == GL11C.GL_TEXTURE) {
            GL11C.glBindTexture(GL11C.GL_TEXTURE_2D, name);
            GL11C.glGetTexLevelParameteriv(GL11C.GL_TEXTURE_2D, 0, GL11C.GL_TEXTURE_INTERNAL_FORMAT, buf);
            GL11C.glBindTexture(GL11C.GL_TEXTURE_2D, 0);
            return buf.get(0);
        }
        if (objectType == GL30C.GL_RENDERBUFFER) {
            GL30C.glBindRenderbuffer(GL30C.GL_RENDERBUFFER, name);
            GL30C.glGetRenderbufferParameteriv(GL30C.GL_RENDERBUFFER, GL30C.GL_RENDERBUFFER_INTERNAL_FORMAT, buf);
            GL30C.glBindRenderbuffer(GL30C.GL_RENDERBUFFER, 0);
            return buf.get(0);
        }
        return 0;
    }

    private MaterialGpu getOrBuildMaterialGpu(PmxInstance instance, int materialId, MaterialInfo mat) {
        MaterialGpu cached = materialGpuCache.get(materialId);
        if (cached != null) return cached;

        MaterialGpu gpu = new MaterialGpu();

        TextureEntry main = getOrLoadTextureEntryLogged(instance, mat.mainTexPath(), false);
        if (main != null && main.rl() != null) {
            gpu.mainTex = main.rl();
            gpu.texMode = main.hasAlpha() ? 2 : 1;
        } else {
            gpu.mainTex = ensureMagentaTexture();
            gpu.texMode = 0;
            LOGGER.warn("[PMX] material {} main texture missing; using fallback. path={}", materialId, mat.mainTexPath());
        }

        TextureEntry sphere = getOrLoadTextureEntryLogged(instance, mat.sphereTexPath(), false);
        if (sphere != null && sphere.rl() != null) {
            gpu.sphereTex = sphere.rl();
            gpu.sphereMode = mat.sphereMode(); // 0/1/2
        } else {
            gpu.sphereTex = ensureMagentaTexture();
            gpu.sphereMode = 0;
            if (mat.sphereTexPath() != null) {
                LOGGER.warn("[PMX] material {} sphere texture missing; using fallback. path={}", materialId, mat.sphereTexPath());
            }
        }

        TextureEntry toon = getOrLoadTextureEntryLogged(instance, mat.toonTexPath(), true);
        if (toon != null && toon.rl() != null) {
            gpu.toonTex = toon.rl();
            gpu.toonMode = 1;
        } else {
            gpu.toonTex = ensureMagentaTexture();
            gpu.toonMode = 0;
            if (mat.toonTexPath() != null) {
                LOGGER.warn("[PMX] material {} toon texture missing; using fallback. path={}", materialId, mat.toonTexPath());
            }
        }

        materialGpuCache.put(materialId, gpu);
        return gpu;
    }

    private TextureEntry getOrLoadTextureEntryLogged(PmxInstance instance, String texPath, boolean clamp) {
        return loadTextureEntryCached(instance, texPath, "pmx/tex_", clamp, true, key -> {
            if (textureLoadFailures.add(key)) {
                LOGGER.warn("[PMX] texture load failed: {}", key);
            }
        });
    }

}
