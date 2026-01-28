package net.Chivent.pmxSteveMod.client.hooks;

import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.client.gui.PmxEmoteWheelScreen;
import net.Chivent.pmxSteveMod.client.gui.PmxModelSelectScreen;
import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.event.TickEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class ClientKeyHooks {
    private ClientKeyHooks() {}
    private static boolean emoteWheelDown;

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
            if (mc.screen == null) {
                mc.setScreen(new PmxEmoteWheelScreen(null));
            }
        }
        emoteWheelDown = wheelDownNow;
    }
}
