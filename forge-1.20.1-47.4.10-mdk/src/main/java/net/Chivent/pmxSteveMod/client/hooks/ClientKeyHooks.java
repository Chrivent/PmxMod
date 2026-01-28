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
import net.minecraftforge.event.TickEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class ClientKeyHooks {
    private ClientKeyHooks() {}
    private static boolean emoteWheelDown;
    private static long emoteWheelNoticeStart = -1L;

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
        if (emoteWheelNoticeStart < 0) return;
        long now = Util.getMillis();
        long elapsed = now - emoteWheelNoticeStart;
        long duration = 1000L;
        if (elapsed >= duration) {
            emoteWheelNoticeStart = -1L;
            return;
        }
        float alpha = 1.0f;
        long fadeStart = 700L;
        if (elapsed > fadeStart) {
            float t = (elapsed - fadeStart) / (float) (duration - fadeStart);
            alpha = 1.0f - Math.min(1.0f, t);
        }
        if (alpha <= 0.01f) return;

        Minecraft mc = Minecraft.getInstance();
        String msg = net.minecraft.network.chat.Component.translatable("pmx.screen.emote_wheel.disabled").getString();
        int color = ((int) (alpha * 255.0f) << 24) | 0xE0E0E0;
        int centerX = mc.getWindow().getGuiScaledWidth() / 2;
        int centerY = mc.getWindow().getGuiScaledHeight() / 2;
        event.getGuiGraphics().drawCenteredString(mc.font, msg, centerX, centerY - 10, color);
    }
}
