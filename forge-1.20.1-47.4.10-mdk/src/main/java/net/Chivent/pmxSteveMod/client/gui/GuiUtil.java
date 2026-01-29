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
    public static final int FOOTER_BUTTON_WIDTH = 110;

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
        ColorFloats c = toColorFloats(color);

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
            builder.vertex(pose, xOuter, yOuter, 0).color(c.r, c.g, c.b, c.a).endVertex();
            builder.vertex(pose, xInner, yInner, 0).color(c.r, c.g, c.b, c.a).endVertex();
        }
        Tesselator.getInstance().end();
        RenderSystem.disableBlend();
    }

    public static void drawWheelRing(GuiGraphics graphics, int cx, int cy, int outerRadius, int innerRadius, int color) {
        int segments = Math.max(48, (int) Math.round(outerRadius * 1.2));
        drawRingSector(graphics, cx, cy, outerRadius, innerRadius, -90.0, 270.0, segments, color);
    }

    public static void drawWheelBoundaries(GuiGraphics graphics, int cx, int cy,
                                           int outerRadius, int innerRadius, int slotCount,
                                           float thickness, int color) {
        double step = 360.0 / slotCount;
        for (int i = 0; i < slotCount; i++) {
            double angle = Math.toRadians(-90.0 + (step * i) - (step / 2.0));
            double cos = Math.cos(angle);
            double sin = Math.sin(angle);
            int x1 = cx + (int) Math.round(cos * innerRadius);
            int y1 = cy + (int) Math.round(sin * innerRadius);
            int x2 = cx + (int) Math.round(cos * outerRadius);
            int y2 = cy + (int) Math.round(sin * outerRadius);
            drawThickLine(graphics, x1, y1, x2, y2, thickness, color);
        }
    }

    public static void drawWheelSelection(GuiGraphics graphics, int cx, int cy,
                                          int outerRadius, int innerRadius, int slotIndex,
                                          int slotCount, int color) {
        double step = 360.0 / slotCount;
        double centerDeg = -90.0 + (step * slotIndex);
        double startDeg = centerDeg - (step / 2.0);
        double endDeg = centerDeg + (step / 2.0);
        int segments = Math.max(12, (int) Math.round(step * 0.6));

        drawRingSector(graphics, cx, cy, outerRadius, innerRadius, startDeg, endDeg, segments, color);
    }

    public static void drawSmoothCircle(GuiGraphics graphics, int cx, int cy, float radius, int color) {
        int outer = Math.max(1, Math.round(radius));
        int segments = Math.max(24, Math.round(outer * 6.0f));
        drawRingSector(graphics, cx, cy, outer, 0, -90.0, 270.0, segments, color);
    }

    public static int getWheelDeadZoneRadius(int wheelRadius) {
        return (int) Math.round(wheelRadius * 0.42);
    }

    public static ColorFloats toColorFloats(int color) {
        float a = ((color >> 24) & 0xFF) / 255.0f;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        return new ColorFloats(r, g, b, a);
    }

    public record ColorFloats(float r, float g, float b, float a) {}

    private static void drawThickLine(GuiGraphics graphics, float x1, float y1, float x2, float y2,
                                      float thickness, int color) {
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = (float) Math.sqrt(dx * dx + dy * dy);
        if (len <= 0.0001f) return;
        float nx = -dy / len;
        float ny = dx / len;
        float half = thickness * 0.5f;
        float ox = nx * half;
        float oy = ny * half;

        ColorFloats c = toColorFloats(color);
        Matrix4f pose = graphics.pose().last().pose();
        BufferBuilder builder = beginColorBuilder();
        builder.vertex(pose, x1 - ox, y1 - oy, 0).color(c.r, c.g, c.b, c.a).endVertex();
        builder.vertex(pose, x1 + ox, y1 + oy, 0).color(c.r, c.g, c.b, c.a).endVertex();
        builder.vertex(pose, x2 + ox, y2 + oy, 0).color(c.r, c.g, c.b, c.a).endVertex();
        builder.vertex(pose, x2 - ox, y2 - oy, 0).color(c.r, c.g, c.b, c.a).endVertex();
        endColorBuilder();
    }

    private static BufferBuilder beginColorBuilder() {
        RenderSystem.setShader(GameRenderer::getPositionColorShader);
        RenderSystem.enableBlend();
        BufferBuilder builder = Tesselator.getInstance().getBuilder();
        builder.begin(VertexFormat.Mode.QUADS, DefaultVertexFormat.POSITION_COLOR);
        return builder;
    }

    private static void endColorBuilder() {
        Tesselator.getInstance().end();
        RenderSystem.disableBlend();
    }
}
