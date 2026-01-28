package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Path;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import org.jetbrains.annotations.NotNull;

public class PmxModelSettingsScreen extends Screen {
    private final Screen parent;
    private final Path modelPath;

    public PmxModelSettingsScreen(Screen parent, Path modelPath) {
        super(Component.translatable("pmx.screen.model_settings.title"));
        this.parent = parent;
        this.modelPath = modelPath;
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        this.renderBackground(graphics);
        graphics.drawCenteredString(this.font, this.title, this.width / 2, 20, 0xFFFFFF);
        super.render(graphics, mouseX, mouseY, partialTick);
    }

    @Override
    public void onClose() {
        Minecraft.getInstance().setScreen(parent);
    }
}
