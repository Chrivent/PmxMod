package net.Chivent.pmxSteveMod.viewer.renderers;

import com.mojang.blaze3d.platform.NativeImage;
import com.mojang.blaze3d.platform.GlStateManager;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.PoseStack;
import net.Chivent.pmxSteveMod.jni.PmxNative;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.texture.DynamicTexture;
import net.minecraft.client.renderer.texture.TextureManager;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import org.lwjgl.opengl.GL11C;
import org.lwjgl.opengl.GL15C;
import org.lwjgl.opengl.GL20C;
import org.lwjgl.opengl.GL30C;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

public abstract class PmxRenderBase {
    public static final float MODEL_SCALE = 0.1f;
    protected record TextureEntry(ResourceLocation rl, boolean hasAlpha) {}

    protected final Map<String, TextureEntry> textureCache = new HashMap<>();
    protected int textureIdCounter = 0;
    protected ResourceLocation magentaTex = null;

    protected void resetTextureCache() {
        textureCache.clear();
        textureIdCounter = 0;
        magentaTex = null;
    }

    protected TextureEntry loadTextureEntry(Path resolved,
                                            String idPrefix,
                                            boolean clamp,
                                            boolean filter) throws IOException {
        try (InputStream in = Files.newInputStream(resolved)) {
            NativeImage img = NativeImage.read(in);
            img.flipY();
            boolean hasAlpha = imageHasAnyAlpha(img);
            DynamicTexture dt = new DynamicTexture(img);
            TextureManager tm = Minecraft.getInstance().getTextureManager();
            String id = idPrefix + Integer.toUnsignedString(++textureIdCounter);
            ResourceLocation rl = tm.register(id, dt);
            if (filter || clamp) {
                RenderSystem.recordRenderCall(() -> {
                    if (filter) {
                        dt.setFilter(true, false);
                    }
                    if (clamp) {
                        dt.bind();
                        GlStateManager._texParameter(3553, 10242, 33071);
                        GlStateManager._texParameter(3553, 10243, 33071);
                    }
                });
            }
            return new TextureEntry(rl, hasAlpha);
        }
    }

    protected TextureEntry loadTextureEntryCached(PmxInstance instance,
                                                  String texPath,
                                                  String idPrefix,
                                                  boolean clamp,
                                                  boolean filter,
                                                  java.util.function.Consumer<String> onFail) {
        if (texPath == null || texPath.isEmpty()) return null;
        Path resolved = resolveTexturePath(instance, texPath);
        if (resolved == null) return null;
        String key = resolved.toString();
        TextureEntry cached = textureCache.get(key);
        if (cached != null) return cached;
        try {
            TextureEntry entry = loadTextureEntry(resolved, idPrefix, clamp, filter);
            textureCache.put(key, entry);
            return entry;
        } catch (Throwable t) {
            if (onFail != null) onFail.accept(key);
            return null;
        }
    }

    protected ResourceLocation ensureMagentaTexture() {
        if (magentaTex != null) return magentaTex;
        try {
            NativeImage img = new NativeImage(1, 1, false);
            img.setPixelRGBA(0, 0, 0xFFFF00FF);
            DynamicTexture dt = new DynamicTexture(img);
            TextureManager tm = Minecraft.getInstance().getTextureManager();
            magentaTex = tm.register("pmx/magenta", dt);
            return magentaTex;
        } catch (Throwable ignored) {
            return null;
        }
    }

    protected boolean shouldSkipRender(PmxInstance instance, boolean requireNormals) {
        if (instance == null || !instance.isReady() || instance.handle() == 0L) return true;
        if (instance.idxBuf() == null || instance.posBuf() == null || instance.uvBuf() == null) return true;
        return requireNormals && instance.nrmBuf() == null;
    }

    protected void applyPlayerBasePose(PoseStack poseStack, AbstractClientPlayer player, float partialTick) {
        if (VanillaPoseUtil.applySleepPose(player, poseStack)) {
            poseStack.scale(MODEL_SCALE, MODEL_SCALE, MODEL_SCALE);
            return;
        }

        float bodyYaw = VanillaPoseUtil.getBodyYawWithHeadClamp(player, partialTick);
        poseStack.mulPose(com.mojang.math.Axis.YP.rotationDegrees(-bodyYaw));
        VanillaPoseUtil.applyBodyTilt(player, partialTick, poseStack);
        VanillaPoseUtil.applyDeathRotation(player, partialTick, poseStack);
        poseStack.scale(MODEL_SCALE, MODEL_SCALE, MODEL_SCALE);
    }

    protected static Path resolveTexturePath(PmxInstance instance, String texPath) {
        try {
            Path p = Paths.get(texPath);
            if (p.isAbsolute()) return p.normalize();
            Path base = instance.pmxBaseDir();
            if (base != null) return base.resolve(p).normalize();
            return p.normalize();
        } catch (Throwable ignored) {
            return null;
        }
    }

    protected record DrawRange(int count, long offsetBytes) {}

    protected static DrawRange getDrawRange(PmxInstance.SubmeshInfo sub, PmxGlMesh mesh) {
        if (sub == null || mesh == null) return null;
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

    protected boolean shouldSkipMeshUpdate(PmxInstance instance, PmxGlMesh mesh) {
        if (instance == null || mesh == null) return false;
        instance.syncCpuBuffersForRender();
        mesh.ensure(instance);
        if (!mesh.ready) return true;
        mesh.updateDynamic(instance);
        return false;
    }

    protected static boolean imageHasAnyAlpha(NativeImage img) {
        int w = img.getWidth();
        int h = img.getHeight();
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int rgba = img.getPixelRGBA(x, y);
                int a = (rgba >>> 24) & 0xFF;
                if (a != 0xFF) return true;
            }
        }
        return false;
    }


    protected static final class PmxGlMesh {
        protected static final int LOC_POS = 0;
        protected static final int LOC_COLOR = 1;
        protected static final int LOC_UV0 = 2;
        protected static final int LOC_UV1 = 3;
        protected static final int LOC_UV2 = 4;
        protected static final int LOC_NRM = 5;

        long ownerHandle = 0L;

        int vao = 0;
        int vboInterleaved = 0;
        int ibo    = 0;

        int vertexCount = 0;
        int indexCount  = 0;
        int elemSize    = 0;
        int glIndexType = GL11C.GL_UNSIGNED_INT;

        boolean ready = false;

        private static final int STRIDE_FLOATS = 8;
        private static final int STRIDE_BYTES = STRIDE_FLOATS * 4;
        private static final int OFFSET_POS = 0;
        private static final int OFFSET_NRM = 3 * 4;
        private static final int OFFSET_UV0 = 6 * 4;

        private ByteBuffer interleavedBytes;
        private FloatBuffer interleavedFloats;

        void ensure(PmxInstance instance) {
            long h = instance.handle();
            if (h == 0L) return;

            int vtxCount = PmxNative.nativeGetVertexCount(h);
            int idxCount = PmxNative.nativeGetIndexCount(h);
            int elemSize = PmxNative.nativeGetIndexElementSize(h);
            int glType = toGlIndexType(elemSize);

            if (ready
                    && ownerHandle == h
                    && vertexCount == vtxCount
                    && indexCount == idxCount
                    && this.elemSize == elemSize
                    && glIndexType == glType) {
                return;
            }

            destroy();

            ByteBuffer idx = instance.idxBuf();
            if (idx == null || vtxCount <= 0 || idxCount <= 0 || elemSize <= 0) return;

            ownerHandle = h;
            vertexCount = vtxCount;
            indexCount = idxCount;
            this.elemSize = elemSize;
            glIndexType = glType;

            vao = GL30C.glGenVertexArrays();
            vboInterleaved = GL15C.glGenBuffers();
            ibo    = GL15C.glGenBuffers();

            GL30C.glBindVertexArray(vao);

            GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, vboInterleaved);
            GL15C.glBufferData(GL15C.GL_ARRAY_BUFFER, (long) vtxCount * STRIDE_BYTES, GL15C.GL_DYNAMIC_DRAW);
            GL20C.glEnableVertexAttribArray(LOC_POS);
            GL20C.glVertexAttribPointer(LOC_POS, 3, GL11C.GL_FLOAT, false, STRIDE_BYTES, OFFSET_POS);
            GL20C.glEnableVertexAttribArray(LOC_UV0);
            GL20C.glVertexAttribPointer(LOC_UV0, 2, GL11C.GL_FLOAT, false, STRIDE_BYTES, OFFSET_UV0);
            GL20C.glEnableVertexAttribArray(LOC_NRM);
            GL20C.glVertexAttribPointer(LOC_NRM, 3, GL11C.GL_FLOAT, false, STRIDE_BYTES, OFFSET_NRM);

            GL20C.glDisableVertexAttribArray(LOC_COLOR);
            GL20C.glDisableVertexAttribArray(LOC_UV1);
            GL20C.glDisableVertexAttribArray(LOC_UV2);

            GL15C.glBindBuffer(GL15C.GL_ELEMENT_ARRAY_BUFFER, ibo);
            ByteBuffer idxDup = idx.duplicate();
            idxDup.rewind();
            GL15C.glBufferData(GL15C.GL_ELEMENT_ARRAY_BUFFER, idxDup, GL15C.GL_STATIC_DRAW);

            GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, 0);
            GL30C.glBindVertexArray(0);

            ready = true;
        }

        void destroy() {
            vao = deleteVao(vao);
            vboInterleaved = deleteBuffer(vboInterleaved);
            ibo = deleteBuffer(ibo);
            interleavedBytes = null;
            interleavedFloats = null;
            ready = false;
            ownerHandle = 0L;
            vertexCount = 0;
            indexCount = 0;
            elemSize = 0;
            glIndexType = GL11C.GL_UNSIGNED_INT;
        }

        void updateDynamic(PmxInstance instance) {
            int vtxCount = vertexCount;
            if (vtxCount <= 0) return;
            ByteBuffer posBuf = instance.posBuf();
            ByteBuffer uvBuf = instance.uvBuf();
            if (posBuf == null || uvBuf == null) return;
            FloatBuffer pos = toFloatBuffer(posBuf);
            FloatBuffer nrm = instance.nrmBuf() != null ? toFloatBuffer(instance.nrmBuf()) : null;
            FloatBuffer uv = toFloatBuffer(uvBuf);
            int requiredFloats = vtxCount * STRIDE_FLOATS;
            int requiredBytes = requiredFloats * 4;
            ensureInterleavedBuffer(requiredBytes);
            interleavedBytes.clear();
            FloatBuffer dst = interleavedFloats;
            dst.clear();
            for (int i = 0; i < vtxCount; i++) {
                int posBase = i * 3;
                int uvBase = i * 2;
                dst.put(pos.get(posBase));
                dst.put(pos.get(posBase + 1));
                dst.put(pos.get(posBase + 2));
                if (nrm != null) {
                    int nrmBase = i * 3;
                    dst.put(nrm.get(nrmBase));
                    dst.put(nrm.get(nrmBase + 1));
                    dst.put(nrm.get(nrmBase + 2));
                } else {
                    dst.put(0.0f);
                    dst.put(1.0f);
                    dst.put(0.0f);
                }
                dst.put(uv.get(uvBase));
                dst.put(uv.get(uvBase + 1));
            }
            interleavedBytes.limit(requiredBytes);
            uploadDynamic(vboInterleaved, interleavedBytes, requiredBytes);
        }

        private static void uploadDynamic(int vbo, ByteBuffer src, long expectedBytes) {
            if (vbo == 0 || src == null) return;
            if (src.capacity() < expectedBytes) return;

            ByteBuffer dup = src.duplicate();
            dup.rewind();

            GL15C.glBindBuffer(GL15C.GL_ARRAY_BUFFER, vbo);
            GL15C.glBufferSubData(GL15C.GL_ARRAY_BUFFER, 0L, dup);
        }

        private void ensureInterleavedBuffer(int requiredBytes) {
            if (interleavedBytes != null && interleavedBytes.capacity() >= requiredBytes) return;
            interleavedBytes = ByteBuffer.allocateDirect(requiredBytes).order(ByteOrder.nativeOrder());
            interleavedFloats = interleavedBytes.asFloatBuffer();
        }

        private static FloatBuffer toFloatBuffer(ByteBuffer buffer) {
            ByteBuffer dup = buffer.duplicate().order(ByteOrder.nativeOrder());
            dup.rewind();
            return dup.asFloatBuffer();
        }

        private static int deleteBuffer(int id) {
            if (id != 0) GL15C.glDeleteBuffers(id);
            return 0;
        }

        private static int deleteVao(int id) {
            if (id != 0) GL30C.glDeleteVertexArrays(id);
            return 0;
        }

        private static int toGlIndexType(int elemSize) {
            return switch (elemSize) {
                case 1 -> GL11C.GL_UNSIGNED_BYTE;
                case 2 -> GL11C.GL_UNSIGNED_SHORT;
                default -> GL11C.GL_UNSIGNED_INT;
            };
        }
    }
}
