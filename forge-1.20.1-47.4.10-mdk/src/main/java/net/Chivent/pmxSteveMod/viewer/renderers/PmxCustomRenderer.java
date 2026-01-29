package net.Chivent.pmxSteveMod.viewer.renderers;

import com.mojang.blaze3d.shaders.Uniform;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.math.Axis;
import com.mojang.logging.LogUtils;
import net.Chivent.pmxSteveMod.client.PmxShaders;
import net.Chivent.pmxSteveMod.jni.PmxNative;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.MaterialInfo;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.SubmeshInfo;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.ShaderInstance;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.client.renderer.texture.TextureManager;
import net.minecraft.resources.ResourceLocation;
import net.minecraft.util.Mth;
import net.minecraft.world.level.Level;
import net.minecraft.client.renderer.LevelRenderer;
import net.minecraft.client.renderer.LightTexture;
import org.joml.Matrix3f;
import org.joml.Matrix4f;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import org.lwjgl.opengl.GL11C;
import org.lwjgl.opengl.GL15C;
import org.lwjgl.opengl.GL20C;
import org.lwjgl.opengl.GL30C;
import org.slf4j.Logger;

import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class PmxCustomRenderer extends PmxRenderBase {

    private static final Logger LOGGER = LogUtils.getLogger();
    private static final boolean FLAT_ENV_LIGHTING = true;

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

    private static final class MeshGpu {
        long ownerHandle = 0L;

        int vao = 0;
        int vboPos = 0;
        int vboNrm = 0;
        int vboUv  = 0;
        int ibo    = 0;

        int vertexCount = 0;
        int indexCount  = 0;
        int elemSize    = 0;
        int glIndexType = GL11C.GL_UNSIGNED_INT;

        boolean ready = false;
    }

    private final MeshGpu mesh = new MeshGpu();

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
    private void set2f(ShaderInstance sh, float x, float y) {
        Uniform u = sh.getUniform("u_ScreenSize");
        if (u != null) u.set(x, y);
    }
    private void set1i(ShaderInstance sh, String name, int v) {
        Uniform u = sh.getUniform(name);
        if (u != null) u.set(v);
    }

    public void onViewerShutdown() {
        RenderSystem.recordRenderCall(() -> {
            destroyMeshGpu();
            resetTextureCache();
            materialGpuCache.clear();
            textureLoadFailures.clear();
            cachedModelVersion = -1L;
        });
    }

    private void destroyMeshGpu() {
        if (mesh.vao != 0) { GL30C.glDeleteVertexArrays(mesh.vao); mesh.vao = 0; }
        if (mesh.vboPos != 0) { GL15C.glDeleteBuffers(mesh.vboPos); mesh.vboPos = 0; }
        if (mesh.vboNrm != 0) { GL15C.glDeleteBuffers(mesh.vboNrm); mesh.vboNrm = 0; }
        if (mesh.vboUv != 0)  { GL15C.glDeleteBuffers(mesh.vboUv);  mesh.vboUv = 0; }
        if (mesh.ibo != 0)    { GL15C.glDeleteBuffers(mesh.ibo);    mesh.ibo = 0; }
        mesh.ready = false;
        mesh.ownerHandle = 0L;
        mesh.vertexCount = 0;
        mesh.indexCount = 0;
        mesh.elemSize = 0;
        mesh.glIndexType = GL11C.GL_UNSIGNED_INT;
    }

    private static int toGlIndexType(int elemSize) {
        return switch (elemSize) {
            case 1 -> GL11C.GL_UNSIGNED_BYTE;
            case 2 -> GL11C.GL_UNSIGNED_SHORT;
            default -> GL11C.GL_UNSIGNED_INT;
        };
    }

    private void ensureMeshGpu(PmxInstance instance) {
        long h = instance.handle();
        if (h == 0L) return;

        int vtxCount = PmxNative.nativeGetVertexCount(h);
        int idxCount = PmxNative.nativeGetIndexCount(h);
        int elemSize = PmxNative.nativeGetIndexElementSize(h);

        int glType = toGlIndexType(elemSize);

        if (mesh.ready
                && mesh.ownerHandle == h
                && mesh.vertexCount == vtxCount
                && mesh.indexCount == idxCount
                && mesh.elemSize == elemSize
                && mesh.glIndexType == glType) {
            return;
        }

        destroyMeshGpu();

        ByteBuffer idx = instance.idxBuf();
        if (idx == null || vtxCount <= 0 || idxCount <= 0 || elemSize <= 0) return;

        mesh.ownerHandle = h;
        mesh.vertexCount = vtxCount;
        mesh.indexCount  = idxCount;
        mesh.elemSize    = elemSize;
        mesh.glIndexType = glType;

        mesh.vao = GL30C.glGenVertexArrays();
        mesh.vboPos = GL15C.glGenBuffers();
        mesh.vboNrm = GL15C.glGenBuffers();
        mesh.vboUv  = GL15C.glGenBuffers();
        mesh.ibo    = GL15C.glGenBuffers();

        GL30C.glBindVertexArray(mesh.vao);

        final int LOC_POS = 0;
        final int LOC_UV0 = 2;
        final int LOC_NRM = 5;

        GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, mesh.vboPos);
        GL15C.glBufferData(GL15C.GL_ARRAY_BUFFER, (long) vtxCount * 3L * 4L, GL15C.GL_DYNAMIC_DRAW);
        GL20C.glEnableVertexAttribArray(LOC_POS);
        GL20C.glVertexAttribPointer(LOC_POS, 3, GL11C.GL_FLOAT, false, 0, 0L);

        GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, mesh.vboUv);
        GL15C.glBufferData(GL15C.GL_ARRAY_BUFFER, (long) vtxCount * 2L * 4L, GL15C.GL_DYNAMIC_DRAW);
        GL20C.glEnableVertexAttribArray(LOC_UV0);
        GL20C.glVertexAttribPointer(LOC_UV0, 2, GL11C.GL_FLOAT, false, 0, 0L);

        GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, mesh.vboNrm);
        GL15C.glBufferData(GL15C.GL_ARRAY_BUFFER, (long) vtxCount * 3L * 4L, GL15C.GL_DYNAMIC_DRAW);
        GL20C.glEnableVertexAttribArray(LOC_NRM);
        GL20C.glVertexAttribPointer(LOC_NRM, 3, GL11C.GL_FLOAT, false, 0, 0L);

        GL15C.glBindBuffer(GL15C.GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
        ByteBuffer idxDup = idx.duplicate();
        idxDup.rewind();
        GL15C.glBufferData(GL15C.GL_ELEMENT_ARRAY_BUFFER, idxDup, GL15C.GL_STATIC_DRAW);

        GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, 0);
        GL30C.glBindVertexArray(0);

        mesh.ready = true;
    }

    private static void uploadDynamic(int vbo, ByteBuffer src, long expectedBytes) {
        if (vbo == 0 || src == null) return;
        if (src.capacity() < expectedBytes) return;

        ByteBuffer dup = src.duplicate();
        dup.rewind();

        GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, vbo);
        GL15C.glBufferSubData(GL15C.GL_ARRAY_BUFFER, 0L, dup);
        GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, 0);
    }

    private void updateDynamicBuffers(PmxInstance instance) {
        if (!mesh.ready) return;
        int vtxCount = mesh.vertexCount;

        uploadDynamic(mesh.vboPos, instance.posBuf(), (long) vtxCount * 3L * 4L);
        uploadDynamic(mesh.vboNrm, instance.nrmBuf(), (long) vtxCount * 3L * 4L);
        uploadDynamic(mesh.vboUv,  instance.uvBuf(),  (long) vtxCount * 2L * 4L);
    }

    public void renderPlayer(PmxInstance instance,
                             AbstractClientPlayer player,
                             float partialTick,
                             PoseStack poseStack) {
        boolean ready = instance.isReady();
        if (!ready || instance.handle() == 0L
                || instance.idxBuf() == null || instance.posBuf() == null
                || instance.nrmBuf() == null || instance.uvBuf() == null) {
            return;
        }

        instance.syncCpuBuffersForRender();

        ShaderInstance sh = PmxShaders.PMX_MMD;
        if (sh == null) return;

        long version = instance.modelVersion();
        if (cachedModelVersion != version) {
            materialGpuCache.clear();
            resetTextureCache();
            textureLoadFailures.clear();
            cachedModelVersion = version;
        }

        ensureMeshGpu(instance);
        if (!mesh.ready) return;

        updateDynamicBuffers(instance);

        poseStack.pushPose();

        float viewYRot = player.getViewYRot(partialTick);
        poseStack.mulPose(Axis.YP.rotationDegrees(-viewYRot));
        poseStack.scale(0.15f, 0.15f, 0.15f);

        Matrix4f pose = poseStack.last().pose();
        float[] lightDir = getSunLightDir(player.level(), partialTick);
        float envLight = getEnvLight(player);

        SubmeshInfo[] subs = instance.submeshes();
        if (subs == null) {
            poseStack.popPose();
            return;
        }

        RenderSystem.enableDepthTest();
        RenderSystem.enableBlend();
        RenderSystem.defaultBlendFunc();
        RenderSystem.depthMask(true);
        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

        GL30C.glBindVertexArray(mesh.vao);

        for (SubmeshInfo sub : subs) {
            drawSubmeshIndexed(instance, sub, pose, lightDir, envLight);
        }

        drawEdgePass(instance, subs, pose);
        GL30C.glBindVertexArray(0);

        RenderSystem.depthMask(true);
        RenderSystem.disableBlend();
        RenderSystem.enableCull();
        RenderSystem.setShaderColor(1f, 1f, 1f, 1f);

        poseStack.popPose();
    }

    private void drawSubmeshIndexed(PmxInstance instance,
                                    SubmeshInfo sm,
                                    Matrix4f pose,
                                    float[] lightDir,
                                    float envLight) {
        if (sm == null) return;

        MaterialInfo mat = instance.material(sm.materialId());
        if (mat == null) return;

        DrawRange range = getDrawRange(sm);
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

        if (FLAT_ENV_LIGHTING) {
            set3f(sh, "u_LightColor", envLight, envLight, envLight);
            set3f(sh, "u_LightDir", 0.0f, 1.0f, 0.0f);
        } else {
            set3f(sh, "u_LightColor", 1f, 1f, 1f);
            set3f(sh, "u_LightDir", lightDir[0], lightDir[1], lightDir[2]);
        }

        if (FLAT_ENV_LIGHTING) {
            set3f(sh, "u_Ambient",
                    mat.ambientR() * envLight,
                    mat.ambientG() * envLight,
                    mat.ambientB() * envLight);
        } else {
            set3f(sh, "u_Ambient", mat.ambientR(), mat.ambientG(), mat.ambientB());
        }

        int rgba = mat.diffuseRGBA();
        float mr = ((rgba >>> 24) & 0xFF) / 255.0f;
        float mg = ((rgba >>> 16) & 0xFF) / 255.0f;
        float mb = ((rgba >>>  8) & 0xFF) / 255.0f;
        set3f(sh, "u_Diffuse", mr, mg, mb);

        if (FLAT_ENV_LIGHTING) {
            set3f(sh, "u_Specular", 0.0f, 0.0f, 0.0f);
            set1f(sh, "u_SpecularPower", 0.0f);
        } else {
            set3f(sh, "u_Specular", mat.specularR(), mat.specularG(), mat.specularB());
            set1f(sh, "u_SpecularPower", mat.specularPower());
        }
        set1f(sh, "u_Alpha", alpha);

        TextureManager texMgr = Minecraft.getInstance().getTextureManager();
        sh.setSampler("Sampler0", texMgr.getTexture(gpu.mainTex));
        sh.setSampler("Sampler1", texMgr.getTexture(gpu.toonTex));
        sh.setSampler("Sampler2", texMgr.getTexture(gpu.sphereTex));

        set1i(sh, "u_TexMode", gpu.texMode);
        float[] tm = mat.texMul();
        float[] ta = mat.texAdd();
        set4f(sh, "u_TexMulFactor", tm[0], tm[1], tm[2], tm[3]);
        set4f(sh, "u_TexAddFactor", ta[0], ta[1], ta[2], ta[3]);

        set1i(sh, "u_SphereTexMode", FLAT_ENV_LIGHTING ? 0 : gpu.sphereMode);
        float[] spMul = mat.sphereMul();
        float[] spAdd = mat.sphereAdd();
        set4f(sh, "u_SphereTexMulFactor", spMul[0], spMul[1], spMul[2], spMul[3]);
        set4f(sh, "u_SphereTexAddFactor", spAdd[0], spAdd[1], spAdd[2], spAdd[3]);

        set1i(sh, "u_ToonTexMode", FLAT_ENV_LIGHTING ? 0 : gpu.toonMode);
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

    private record DrawRange(int count, long offsetBytes) {}

    private DrawRange getDrawRange(SubmeshInfo sub) {
        if (sub == null) return null;
        int begin = sub.beginIndex();
        int count = sub.indexCount();
        if (begin < 0 || count <= 0) return null;
        int maxCount = Math.max(0, mesh.indexCount - begin);
        if (count > maxCount) count = maxCount;
        count -= (count % 3);
        if (count <= 0) return null;
        long offsetBytes = (long) begin * (long) mesh.elemSize;
        return new DrawRange(count, offsetBytes);
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
        set2f(edgeShader, window.getScreenWidth(), window.getScreenHeight());

        edgeShader.apply();

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

            DrawRange range = getDrawRange(sub);
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

    private static float getEnvLight(AbstractClientPlayer player) {
        Level level = player.level();
        int packed = LevelRenderer.getLightColor(level, player.blockPosition());
        int sky = LightTexture.sky(packed);
        int block = LightTexture.block(packed);
        float skyBr = LightTexture.getBrightness(level.dimensionType(), sky);
        float blockBr = LightTexture.getBrightness(level.dimensionType(), block);
        return Mth.clamp(Math.max(skyBr, blockBr), 0.0f, 1.0f);
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
