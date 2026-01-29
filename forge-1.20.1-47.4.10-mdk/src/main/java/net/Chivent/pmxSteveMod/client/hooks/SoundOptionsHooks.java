package net.Chivent.pmxSteveMod.client.hooks;

import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.client.settings.PmxSoundSettingsStore;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraft.client.OptionInstance;
import net.minecraft.client.gui.components.OptionsList;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.client.event.ScreenEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class SoundOptionsHooks {
    private SoundOptionsHooks() {}

    @SubscribeEvent
    public static void onScreenInit(ScreenEvent.Init.Post event) {
        Screen screen = event.getScreen();
        if (!isSoundOptionsScreen(screen)) return;

        OptionsList list = findOptionsList(screen);
        if (list == null) return;
        int before = list.children().size();
        list.addBig(buildPmxVolumeOption());
        int after = list.children().size();
        if (after > before) {
            moveEntry(list, after - 1);
        }
    }

    private static OptionsList findOptionsList(Screen screen) {
        for (var child : screen.children()) {
            if (child instanceof OptionsList list) {
                return list;
            }
        }
        return null;
    }

    private static boolean isSoundOptionsScreen(Screen screen) {
        return screen != null && "SoundOptionsScreen".equals(screen.getClass().getSimpleName());
    }

    private static OptionInstance<Double> buildPmxVolumeOption() {
        double initial = PmxSoundSettingsStore.get().getMusicVolume();
        OptionInstance.TooltipSupplier<Double> tooltip = OptionInstance.noTooltip();
        OptionInstance.CaptionBasedToString<Double> toString = (caption, value) -> {
            int percent = (int) Math.round(value * 100.0);
            return Component.literal(caption.getString() + ": " + percent + "%");
        };
        return new OptionInstance<>(
                "pmx.settings.sound_volume",
                tooltip,
                toString,
                OptionInstance.UnitDouble.INSTANCE,
                initial,
                value -> {
                    float v = Math.max(0.0f, Math.min(1.0f, value.floatValue()));
                    PmxSoundSettingsStore.get().setMusicVolume(v);
                    PmxViewer.get().instance().setMusicVolume(v);
                }
        );
    }

    @SuppressWarnings("unchecked")
    private static void moveEntry(OptionsList list, int from) {
        java.util.List<Object> entries = (java.util.List<Object>) (java.util.List<?>) list.children();
        if (from < 0 || from >= entries.size()) return;
        Object entry = entries.remove(from);
        int target = Math.min(1, entries.size());
        entries.add(target, entry);
    }
}
