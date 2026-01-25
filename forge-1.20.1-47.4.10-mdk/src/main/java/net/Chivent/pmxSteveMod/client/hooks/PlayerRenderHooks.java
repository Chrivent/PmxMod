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
        // 여기에 조건(예: 내 모드가 활성일 때만, 특정 플레이어만 등) 붙이면 됨

        // 바닐라 렌더 취소
        event.setCanceled(true);

        // 우리 렌더로 대체 (아직 구현은 비워둠)
        PmxViewer.get().renderPlayer(
                (net.minecraft.client.player.AbstractClientPlayer) event.getEntity(),
                event.getRenderer(),
                event.getPartialTick(),
                event.getPoseStack(),
                event.getMultiBufferSource(),
                event.getPackedLight()
        );
    }
}
