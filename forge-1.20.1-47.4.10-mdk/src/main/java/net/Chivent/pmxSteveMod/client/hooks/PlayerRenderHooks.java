package net.Chivent.pmxSteveMod.client.hooks;

import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.renderers.PmxVanillaRenderer;
import net.Chivent.pmxSteveMod.viewer.renderers.PmxOculusRenderer;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.Chivent.pmxSteveMod.client.util.PmxShaderPackUtil;
import net.minecraft.client.renderer.entity.EntityRenderer;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.client.event.RenderPlayerEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;
import java.lang.reflect.Field;
import java.util.IdentityHashMap;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class PlayerRenderHooks {
    private static final PmxVanillaRenderer VANILLA_RENDERER = new PmxVanillaRenderer();
    private static final PmxOculusRenderer OCULUS_RENDERER = new PmxOculusRenderer();
    private static final IdentityHashMap<EntityRenderer<?>, ShadowState> SHADOW_STATES = new IdentityHashMap<>();
    private static Field shadowRadiusField;
    private static Field shadowStrengthField;
    private static boolean triedShadowFields;
    private PlayerRenderHooks() {}

    @SubscribeEvent
    public static void onRenderPlayerPre(RenderPlayerEvent.Pre event) {
        PmxViewer viewer = PmxViewer.get();
        if (!viewer.isPmxVisible()) {
            restoreShadow(event.getRenderer());
            return;
        }
        PmxInstance instance = viewer.instance();
        boolean ready = instance.isReady();
        if (!ready) {
            restoreShadow(event.getRenderer());
            return;
        }

        disableShadow(event.getRenderer());

        event.setCanceled(true);

        net.minecraft.client.player.AbstractClientPlayer player =
                (net.minecraft.client.player.AbstractClientPlayer) event.getEntity();
        if (PmxShaderPackUtil.isOculusLoaded()) {
            OCULUS_RENDERER.renderPlayer(instance, player, event.getPartialTick(),
                    event.getPoseStack(), event.getPackedLight());
        } else {
            VANILLA_RENDERER.renderPlayer(instance, player, event.getPartialTick(),
                    event.getPoseStack(), event.getPackedLight());
        }

        VANILLA_RENDERER.renderSpinAttackEffect(instance, player, event.getPartialTick(),
                event.getPoseStack(), event.getMultiBufferSource(), event.getPackedLight());
    }

    public static void shutdownRenderer() {
        VANILLA_RENDERER.onViewerShutdown();
        OCULUS_RENDERER.onViewerShutdown();
    }


    private static void disableShadow(EntityRenderer<?> renderer) {
        ensureShadowFields();
        if (shadowRadiusField == null && shadowStrengthField == null) return;
        ShadowState state = SHADOW_STATES.get(renderer);
        if (state == null) {
            Float radius = readShadowField(renderer, shadowRadiusField);
            Float strength = readShadowField(renderer, shadowStrengthField);
            if (radius == null && strength == null) return;
            state = new ShadowState(radius != null ? radius : 0.0f, strength != null ? strength : 1.0f);
            SHADOW_STATES.put(renderer, state);
        }
        writeShadowField(renderer, shadowRadiusField, 0.0f);
        writeShadowField(renderer, shadowStrengthField, 0.0f);
    }

    private static void restoreShadow(EntityRenderer<?> renderer) {
        ShadowState state = SHADOW_STATES.remove(renderer);
        if (state == null) return;
        writeShadowField(renderer, shadowRadiusField, state.radius());
        writeShadowField(renderer, shadowStrengthField, state.strength());
    }

    private static void ensureShadowFields() {
        if (triedShadowFields) return;
        triedShadowFields = true;
        try {
            shadowRadiusField = EntityRenderer.class.getDeclaredField("shadowRadius");
            shadowRadiusField.setAccessible(true);
        } catch (Throwable ignored) {
            shadowRadiusField = null;
        }
        try {
            shadowStrengthField = EntityRenderer.class.getDeclaredField("shadowStrength");
            shadowStrengthField.setAccessible(true);
        } catch (Throwable ignored) {
            shadowStrengthField = null;
        }
    }

    private static Float readShadowField(EntityRenderer<?> renderer, Field field) {
        if (field == null) return null;
        try {
            return field.getFloat(renderer);
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static void writeShadowField(EntityRenderer<?> renderer, Field field, float value) {
        if (field == null) return;
        try {
            field.setFloat(renderer, value);
        } catch (Throwable ignored) {
        }
    }

    private record ShadowState(float radius, float strength) {}
}
