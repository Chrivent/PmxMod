package net.Chivent.pmxSteveMod.viewer;

import com.mojang.blaze3d.vertex.PoseStack;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.client.renderer.entity.player.PlayerRenderer;

public class PmxViewer {
    private static final PmxViewer INSTANCE = new PmxViewer();
    public static PmxViewer get() { return INSTANCE; }

    private boolean ready = false;

    private PmxViewer() {}

    public boolean isReady() {
        return ready;
    }

    public void init() {
        // TODO: implement later
    }

    public void tick() {
        // TODO: implement later
    }

    public void renderPlayer(AbstractClientPlayer player,
                             PlayerRenderer vanillaRenderer,
                             float partialTick,
                             PoseStack poseStack,
                             MultiBufferSource buffers,
                             int packedLight) {
        // TODO: implement later
    }

    public void onReload() {
        // TODO: implement later
    }

    public void shutdown() {
        // TODO: implement later
        ready = false;
    }
}
