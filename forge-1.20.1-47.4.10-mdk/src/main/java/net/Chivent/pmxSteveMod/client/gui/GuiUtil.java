package net.Chivent.pmxSteveMod.client.gui;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import net.minecraft.resources.ResourceLocation;
import net.minecraft.world.inventory.InventoryMenu;

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
}
