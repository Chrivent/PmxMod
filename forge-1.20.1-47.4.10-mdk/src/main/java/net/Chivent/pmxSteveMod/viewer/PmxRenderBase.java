package net.Chivent.pmxSteveMod.viewer;

import com.mojang.blaze3d.platform.NativeImage;
import com.mojang.blaze3d.platform.GlStateManager;
import com.mojang.blaze3d.systems.RenderSystem;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.texture.DynamicTexture;
import net.minecraft.client.renderer.texture.TextureManager;
import net.minecraft.resources.ResourceLocation;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

public abstract class PmxRenderBase {
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
}
