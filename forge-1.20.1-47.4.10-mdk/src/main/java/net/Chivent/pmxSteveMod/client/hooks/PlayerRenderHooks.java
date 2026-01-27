package net.Chivent.pmxSteveMod.client.hooks;

import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.client.event.RenderPlayerEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class PlayerRenderHooks {
    private PlayerRenderHooks() {}

    @SubscribeEvent
    public static void onRenderPlayerPre(RenderPlayerEvent.Pre event) {
        PmxViewer viewer = PmxViewer.get();
        if (!viewer.isReady()) return;

        event.setCanceled(true);

        viewer.tickRender();
        viewer.renderer().renderPlayer(
                viewer,
                (net.minecraft.client.player.AbstractClientPlayer) event.getEntity(),
                event.getPartialTick(),
                event.getPoseStack()
        );
    }
}
