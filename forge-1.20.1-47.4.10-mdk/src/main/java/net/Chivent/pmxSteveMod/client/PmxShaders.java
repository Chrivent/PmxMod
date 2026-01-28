package net.Chivent.pmxSteveMod.client;

import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.minecraft.client.renderer.ShaderInstance;
import net.minecraft.resources.ResourceLocation;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.client.event.RegisterShadersEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;

import java.io.IOException;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.MOD, value = Dist.CLIENT)
public final class PmxShaders {
    private PmxShaders() {}

    public static ShaderInstance PMX_MMD;
    public static ShaderInstance PMX_EDGE;

    @SubscribeEvent
    public static void onRegisterShaders(RegisterShadersEvent event) throws IOException {
        event.registerShader(
                new ShaderInstance(
                        event.getResourceProvider(),
                        ResourceLocation.fromNamespaceAndPath(PmxSteveMod.MOD_ID, "pmx_mmd"),
                        DefaultVertexFormat.NEW_ENTITY
                ),
                s -> PMX_MMD = s
        );
        event.registerShader(
                new ShaderInstance(
                        event.getResourceProvider(),
                        ResourceLocation.fromNamespaceAndPath(PmxSteveMod.MOD_ID, "pmx_edge"),
                        DefaultVertexFormat.NEW_ENTITY
                ),
                s -> PMX_EDGE = s
        );
    }
}
