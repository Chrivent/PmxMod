package net.Chivent.pmxSteveMod.viewer;

import com.mojang.blaze3d.platform.NativeImage;
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.VertexConsumer;
import com.mojang.math.Axis;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.MaterialInfo;
import net.Chivent.pmxSteveMod.viewer.PmxInstance.SubmeshInfo;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.LightTexture;
import net.minecraft.client.renderer.RenderType;
import net.minecraft.client.renderer.texture.DynamicTexture;
import net.minecraft.client.renderer.texture.TextureManager;
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import org.joml.Matrix3f;
import org.joml.Matrix4f;

import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

public class PmxVanillaRenderer {
    private record TextureEntry(ResourceLocation rl, boolean hasAlpha) {}
    private final Map<String, TextureEntry> textureCache = new HashMap<>();
    private int textureIdCounter = 0;
    private ResourceLocation magentaTex = null;

    public void onViewerShutdown() {
        textureCache.clear();
        textureIdCounter = 0;
        magentaTex = null;
    }

    public void renderPlayer(PmxInstance instance,
                             AbstractClientPlayer player,
                             float partialTick,
                             PoseStack poseStack) {
        if (!instance.isReady() || instance.handle() == 0L) return;
        if (instance.idxBuf() == null || instance.posBuf() == null
                || instance.nrmBuf() == null || instance.uvBuf() == null) {
            return;
        }

        instance.syncCpuBuffersForRender();

        poseStack.pushPose();
        float viewYRot = player.getViewYRot(partialTick);
        poseStack.mulPose(Axis.YP.rotationDegrees(-viewYRot));
        poseStack.scale(0.15f, 0.15f, 0.15f);

        Matrix4f pose = poseStack.last().pose();
        Matrix3f normalMat = poseStack.last().normal();

        ByteBuffer posBuf = instance.posBuf().duplicate().order(ByteOrder.nativeOrder());
        ByteBuffer uvBuf = instance.uvBuf().duplicate().order(ByteOrder.nativeOrder());
        ByteBuffer idxBuf = instance.idxBuf().duplicate().order(ByteOrder.nativeOrder());

        int elemSize = net.Chivent.pmxSteveMod.jni.PmxNative.nativeGetIndexElementSize(instance.handle());
        int light = LightTexture.FULL_BRIGHT;
        MultiBufferSource.BufferSource buffer = Minecraft.getInstance().renderBuffers().bufferSource();
        SubmeshInfo[] subs = instance.submeshes();
        if (subs != null) {
            for (SubmeshInfo sub : subs) {
                MaterialInfo mat = instance.material(sub.materialId());
                if (mat == null) continue;
                float alpha = mat.alpha();
                if (alpha <= 0.0f) continue;

                TextureEntry tex = getOrLoadMainTexture(instance, mat.mainTexPath());
                ResourceLocation rl = tex != null ? tex.rl : ensureMagentaTexture();
                boolean translucent = alpha < 0.999f;
                RenderType type = rl != null ? getRenderType(rl, translucent) : null;
                VertexConsumer vc = null;
                if (type != null) {
                    vc = buffer.getBuffer(type);
                }

                if (vc == null) continue;
                drawSubmesh(vc, sub, posBuf, uvBuf, idxBuf, elemSize,
                        pose, normalMat,
                        alpha, light);
            }
        }

        buffer.endBatch();
        poseStack.popPose();
    }

    private void drawSubmesh(VertexConsumer vc,
                             SubmeshInfo sub,
                             ByteBuffer posBuf,
                             ByteBuffer uvBuf,
                             ByteBuffer idxBuf,
                             int elemSize,
                             Matrix4f pose,
                             Matrix3f normalMat,
                             float alpha,
                             int light) {
        int begin = sub.beginIndex();
        int count = sub.indexCount();
        if (begin < 0 || count <= 0) return;
        count -= (count % 3);
        if (count <= 0) return;

        for (int i = 0; i + 2 < count; i += 3) {
            int idx0 = readIndex(idxBuf, elemSize, begin + i);
            int idx1 = readIndex(idxBuf, elemSize, begin + i + 1);
            int idx2 = readIndex(idxBuf, elemSize, begin + i + 2);
            addVertex(vc, pose, normalMat, posBuf, uvBuf, idx0, alpha, light);
            addVertex(vc, pose, normalMat, posBuf, uvBuf, idx1, alpha, light);
            addVertex(vc, pose, normalMat, posBuf, uvBuf, idx2, alpha, light);
            addVertex(vc, pose, normalMat, posBuf, uvBuf, idx2, alpha, light);
        }
    }

    private void addVertex(VertexConsumer vc,
                           Matrix4f pose,
                           Matrix3f normalMat,
                           ByteBuffer posBuf,
                           ByteBuffer uvBuf,
                           int index,
                           float a,
                           int light) {
        int posBase = index * 12;
        float x = posBuf.getFloat(posBase);
        float y = posBuf.getFloat(posBase + 4);
        float z = posBuf.getFloat(posBase + 8);

        int uvBase = index * 8;
        float u = uvBuf.getFloat(uvBase);
        float v = uvBuf.getFloat(uvBase + 4);

        vc.vertex(pose, x, y, z)
                .color(1.0f, 1.0f, 1.0f, a)
                .uv(u, v)
                .overlayCoords(net.minecraft.client.renderer.texture.OverlayTexture.NO_OVERLAY)
                .uv2(light)
                .normal(normalMat, 0.0f, 1.0f, 0.0f)
                .endVertex();
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
        if (!translucent) {
            return RenderType.dragonExplosionAlpha(rl);
        }
        RenderType emissive = tryGetEmissiveRenderType(rl);
        if (emissive != null) return emissive;
        RenderType noShade = tryGetNoShadeRenderType(rl);
        if (noShade != null) return noShade;
        return RenderType.entityTranslucent(rl);
    }

    private RenderType tryGetEmissiveRenderType(ResourceLocation rl) {
        String[] methodNames = new String[] {"entityTranslucentEmissive", "entityTranslucentCullEmissive"};
        for (String name : methodNames) {
            try {
                java.lang.reflect.Method method = RenderType.class.getMethod(name, ResourceLocation.class);
                Object out = method.invoke(null, rl);
                if (out instanceof RenderType rt) {
                    return rt;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }

    private RenderType tryGetNoShadeRenderType(ResourceLocation rl) {
        String[] methodNames = new String[] {"entityTranslucentNoOutline", "entityTranslucent", "entityTranslucentCull"};
        for (String name : methodNames) {
            try {
                java.lang.reflect.Method method = RenderType.class.getMethod(name, ResourceLocation.class);
                Object out = method.invoke(null, rl);
                if (out instanceof RenderType rt) {
                    return rt;
                }
            } catch (ReflectiveOperationException ignored) {
            }
        }
        return null;
    }


    private TextureEntry getOrLoadMainTexture(PmxInstance instance, String texPath) {
        if (texPath == null || texPath.isEmpty()) return null;
        Path resolved = resolveTexturePath(instance, texPath);
        if (resolved == null) return null;
        String key = resolved.toString();
        TextureEntry cached = textureCache.get(key);
        if (cached != null) return cached;

        try (InputStream in = Files.newInputStream(resolved)) {
            NativeImage img = NativeImage.read(in);
            img.flipY();
            boolean hasAlpha = imageHasAnyAlpha(img);
            DynamicTexture dt = new DynamicTexture(img);
            TextureManager tm = Minecraft.getInstance().getTextureManager();
            String id = "pmx/vanilla_tex_" + Integer.toUnsignedString(++textureIdCounter);
            ResourceLocation rl = tm.register(id, dt);
            TextureEntry entry = new TextureEntry(rl, hasAlpha);
            textureCache.put(key, entry);
            return entry;
        } catch (Throwable ignored) {
            return null;
        }
    }

    private ResourceLocation ensureMagentaTexture() {
        if (magentaTex != null) return magentaTex;
        try {
            NativeImage img = new NativeImage(1, 1, false);
            img.setPixelRGBA(0, 0, 0xFFFF00FF);
            DynamicTexture dt = new DynamicTexture(img);
            TextureManager tm = Minecraft.getInstance().getTextureManager();
            magentaTex = tm.register("pmx/vanilla_magenta", dt);
            return magentaTex;
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static Path resolveTexturePath(PmxInstance instance, String texPath) {
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

    private static boolean imageHasAnyAlpha(NativeImage img) {
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
}
