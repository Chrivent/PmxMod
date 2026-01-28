package net.Chivent.pmxSteveMod.client.gui;

import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.BufferBuilder;
import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import com.mojang.blaze3d.vertex.Tesselator;
import com.mojang.blaze3d.vertex.VertexFormat;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import net.minecraft.resources.ResourceLocation;
import net.minecraft.world.inventory.InventoryMenu;
import org.joml.Matrix4f;

public final class GuiUtil {
    public static final ResourceLocation DEFAULT_SPRITE_ID =
            ResourceLocation.fromNamespaceAndPath("minecraft", "block/obsidian");

    private GuiUtil() {}

    public static void renderDefaultBackground(GuiGraphics graphics, int width, int height) {
        renderTiledBackground(graphics, width, height, DEFAULT_SPRITE_ID, 32, 32, 0x55000000, 0x77000000);
    }

    public static void renderTiledBackground(GuiGraphics graphics, int width, int height,
                                             ResourceLocation spriteId, int topPad, int bottomPad,
                                             int gradientTopColor, int gradientBottomColor) {
        Minecraft mc = Minecraft.getInstance();
        TextureAtlasSprite sprite = mc.getTextureAtlas(InventoryMenu.BLOCK_ATLAS).apply(spriteId);
        int tile = 16;
        int top = Math.max(0, topPad);
        int bottom = Math.max(top, height - bottomPad);
        renderTileBand(graphics, sprite, width, tile, 0, top);
        renderTileBand(graphics, sprite, width, tile, bottom, height);
        if (bottom > top) {
            graphics.fillGradient(0, top, width, bottom, gradientTopColor, gradientBottomColor);
        }
    }

    public static void renderTileBand(GuiGraphics graphics, TextureAtlasSprite sprite, int width,
                                      int tile, int startY, int endY) {
        for (int y = startY; y < endY; y += tile) {
            int h = Math.min(tile, endY - y);
            for (int x = 0; x < width; x += tile) {
                int w = Math.min(tile, width - x);
                graphics.blit(x, y, 0, w, h, sprite);
            }
        }
    }

    public static void drawRingSector(GuiGraphics graphics, int cx, int cy, int outerRadius, int innerRadius,
                                      double startDeg, double endDeg, int segments, int color) {
        float a = ((color >> 24) & 0xFF) / 255.0f;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;

        Matrix4f pose = graphics.pose().last().pose();
        RenderSystem.setShader(GameRenderer::getPositionColorShader);
        RenderSystem.enableBlend();
        BufferBuilder builder = Tesselator.getInstance().getBuilder();
        builder.begin(VertexFormat.Mode.TRIANGLE_STRIP, DefaultVertexFormat.POSITION_COLOR);
        for (int i = 0; i <= segments; i++) {
            double t = i / (double) segments;
            double deg = startDeg + (endDeg - startDeg) * t;
            double rad = Math.toRadians(deg);
            float cos = (float) Math.cos(rad);
            float sin = (float) Math.sin(rad);
            float xOuter = cx + cos * outerRadius;
            float yOuter = cy + sin * outerRadius;
            float xInner = cx + cos * innerRadius;
            float yInner = cy + sin * innerRadius;
            builder.vertex(pose, xOuter, yOuter, 0).color(r, g, b, a).endVertex();
            builder.vertex(pose, xInner, yInner, 0).color(r, g, b, a).endVertex();
        }
        Tesselator.getInstance().end();
        RenderSystem.disableBlend();
    }
}
