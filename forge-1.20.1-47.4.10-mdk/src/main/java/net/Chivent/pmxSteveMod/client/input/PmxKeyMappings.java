package net.Chivent.pmxSteveMod.client.input;

import com.mojang.blaze3d.platform.InputConstants;
import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.minecraft.client.KeyMapping;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.client.event.RegisterKeyMappingsEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.MOD, value = Dist.CLIENT)
public final class PmxKeyMappings {
    public static final String CATEGORY = "key.categories.pmxstevemod";
    public static final String KEY_OPEN_MENU = "key.pmxstevemod.open_menu";

    public static final KeyMapping OPEN_MENU = new KeyMapping(
            KEY_OPEN_MENU,
            InputConstants.Type.KEYSYM,
            InputConstants.KEY_K,
            CATEGORY
    );

    private PmxKeyMappings() {}

    @SubscribeEvent
    public static void onRegisterKeyMappings(RegisterKeyMappingsEvent event) {
        event.register(OPEN_MENU);
    }
}
