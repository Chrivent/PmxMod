package net.Chivent.pmxSteveMod.client.hooks;

import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.event.TickEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class ClientTickHooks {
    private ClientTickHooks() {}

    @SubscribeEvent
    public static void onRenderTick(TickEvent.RenderTickEvent event) {
        if (event.phase != TickEvent.Phase.END) return;
        PmxViewer viewer = PmxViewer.get();
        if (!viewer.isPmxVisible()) return;
        PmxInstance instance = viewer.instance();
        if (!instance.isReady()) return;
        instance.tickRender();
        viewer.handleMotionEnd();
    }

    @SubscribeEvent
    public static void onClientTick(TickEvent.ClientTickEvent event) {

    }
}
