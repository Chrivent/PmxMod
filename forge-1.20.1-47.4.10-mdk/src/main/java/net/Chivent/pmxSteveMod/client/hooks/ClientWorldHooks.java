package net.Chivent.pmxSteveMod.client.hooks;

import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.event.GameShuttingDownEvent;
import net.minecraftforge.event.level.LevelEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class ClientWorldHooks {
    private ClientWorldHooks() {}

    @SubscribeEvent
    public static void onLevelLoad(LevelEvent.Load event) {
        if (!event.getLevel().isClientSide()) return;
        PmxViewer.get().init();
    }

    @SubscribeEvent
    public static void onLevelUnload(LevelEvent.Unload event) {
        if (!event.getLevel().isClientSide()) return;
        PlayerRenderHooks.shutdownRenderer();
        PmxViewer.get().shutdown();
    }

    @SubscribeEvent
    public static void onGameShuttingDown(GameShuttingDownEvent event) {
        PlayerRenderHooks.shutdownRenderer();
        PmxViewer.get().shutdown();
    }
}
