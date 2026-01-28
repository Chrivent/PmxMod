package net.Chivent.pmxSteveMod.client.hooks;

import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.client.gui.PmxEmoteWheelScreen;
import net.Chivent.pmxSteveMod.client.gui.PmxModelSelectScreen;
import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraft.Util;
import net.minecraftforge.client.event.RenderGuiOverlayEvent;
import net.minecraftforge.client.gui.overlay.VanillaGuiOverlay;
import net.minecraftforge.event.TickEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class ClientKeyHooks {
    private ClientKeyHooks() {}
    private static boolean emoteWheelDown;
    private static long emoteWheelNoticeStart = -1L;
    private static final long EMOTE_WHEEL_NOTICE_MS = 1000L;
    private static final long EMOTE_WHEEL_FADE_START_MS = 500L;

    @SubscribeEvent
    public static void onClientTick(TickEvent.ClientTickEvent event) {
        if (event.phase != TickEvent.Phase.END)
            return;
        Minecraft mc = Minecraft.getInstance();
        if (mc.level == null)
            return;
        while (PmxKeyMappings.OPEN_MENU.consumeClick()) {
            Screen parent = mc.screen;
            mc.setScreen(new PmxModelSelectScreen(parent));
        }

        boolean wheelDownNow = PmxKeyMappings.EMOTE_WHEEL.isDown();
        if (wheelDownNow && !emoteWheelDown) {
            if (mc.screen == null && net.Chivent.pmxSteveMod.viewer.PmxViewer.get().isPmxVisible()) {
                mc.setScreen(new PmxEmoteWheelScreen(null));
            } else if (mc.screen == null) {
                emoteWheelNoticeStart = Util.getMillis();
            }
        }
        emoteWheelDown = wheelDownNow;
    }

    @SubscribeEvent
    public static void onRenderGuiOverlay(RenderGuiOverlayEvent.Post event) {
        if (!event.getOverlay().id().equals(VanillaGuiOverlay.PLAYER_LIST.id())) return;
        if (emoteWheelNoticeStart < 0) return;
        long elapsed = Util.getMillis() - emoteWheelNoticeStart;
        if (elapsed >= EMOTE_WHEEL_NOTICE_MS) {
            emoteWheelNoticeStart = -1L;
            return;
        }
        float alpha = computeNoticeAlpha(elapsed);
        if (alpha <= 0.01f) return;

        Minecraft mc = Minecraft.getInstance();
        String msg = net.minecraft.network.chat.Component.translatable("pmx.screen.emote_wheel.disabled").getString();
        drawCenteredNotice(event.getGuiGraphics(), mc, msg, alpha);
    }

    private static float computeNoticeAlpha(long elapsedMs) {
        if (elapsedMs <= ClientKeyHooks.EMOTE_WHEEL_FADE_START_MS) return 1.0f;
        if (elapsedMs >= ClientKeyHooks.EMOTE_WHEEL_NOTICE_MS) return 0.0f;
        float t = (elapsedMs - ClientKeyHooks.EMOTE_WHEEL_FADE_START_MS) /
                (float) (ClientKeyHooks.EMOTE_WHEEL_NOTICE_MS - ClientKeyHooks.EMOTE_WHEEL_FADE_START_MS);
        return 1.0f - Math.min(1.0f, t);
    }

    private static void drawCenteredNotice(net.minecraft.client.gui.GuiGraphics graphics, Minecraft mc,
                                           String msg, float alpha) {
        int centerX = mc.getWindow().getGuiScaledWidth() / 2;
        int centerY = mc.getWindow().getGuiScaledHeight() / 2;
        int color = ((int) (alpha * 255.0f) << 24) | 0xE0E0E0;
        com.mojang.blaze3d.systems.RenderSystem.enableBlend();
        com.mojang.blaze3d.systems.RenderSystem.defaultBlendFunc();
        graphics.drawCenteredString(mc.font, msg, centerX, centerY - 10, color);
        com.mojang.blaze3d.systems.RenderSystem.disableBlend();
    }
}
