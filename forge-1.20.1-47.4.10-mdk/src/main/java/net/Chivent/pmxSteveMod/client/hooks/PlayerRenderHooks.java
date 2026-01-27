package net.Chivent.pmxSteveMod.client.hooks;

import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxRenderer;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.client.event.RenderPlayerEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class PlayerRenderHooks {
    private static final PmxRenderer RENDERER = new PmxRenderer();
    private PlayerRenderHooks() {}

    @SubscribeEvent
    public static void onRenderPlayerPre(RenderPlayerEvent.Pre event) {
        PmxInstance instance = PmxViewer.get().instance();
        boolean ready = instance.isReady();
        if (!ready) return;

        event.setCanceled(true);

        instance.tickRender();
        RENDERER.renderPlayer(
                instance,
                (net.minecraft.client.player.AbstractClientPlayer) event.getEntity(),
                event.getPartialTick(),
                event.getPoseStack()
        );
    }

    public static void shutdownRenderer() {
        RENDERER.onViewerShutdown();
    }
}
