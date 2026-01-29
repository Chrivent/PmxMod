package net.Chivent.pmxSteveMod.client.hooks;

import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.renderers.PmxCustomRenderer;
import net.Chivent.pmxSteveMod.viewer.renderers.PmxVanillaRenderer;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.client.event.RenderPlayerEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class PlayerRenderHooks {
    private static final boolean USE_VANILLA_RENDERER = true;
    private static final PmxCustomRenderer RENDERER = new PmxCustomRenderer();
    private static final PmxVanillaRenderer VANILLA_RENDERER = new PmxVanillaRenderer();
    private PlayerRenderHooks() {}

    @SubscribeEvent
    public static void onRenderPlayerPre(RenderPlayerEvent.Pre event) {
        PmxViewer viewer = PmxViewer.get();
        if (!viewer.isPmxVisible()) return;
        PmxInstance instance = viewer.instance();
        boolean ready = instance.isReady();
        if (!ready) return;

        event.setCanceled(true);

        net.minecraft.client.player.AbstractClientPlayer player =
                (net.minecraft.client.player.AbstractClientPlayer) event.getEntity();
        if (USE_VANILLA_RENDERER) {
            VANILLA_RENDERER.renderPlayer(instance, player, event.getPartialTick(),
                    event.getPoseStack(), event.getMultiBufferSource(), event.getPackedLight());
        } else {
            RENDERER.renderPlayer(instance, player, event.getPartialTick(), event.getPoseStack());
        }
    }

    public static void shutdownRenderer() {
        RENDERER.onViewerShutdown();
        VANILLA_RENDERER.onViewerShutdown();
    }
}
