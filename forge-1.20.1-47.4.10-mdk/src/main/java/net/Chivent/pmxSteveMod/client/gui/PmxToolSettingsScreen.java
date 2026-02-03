package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Path;

import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import org.jetbrains.annotations.NotNull;

public class PmxToolSettingsScreen extends Screen {
    private final Screen parent;
    @SuppressWarnings("unused")
    private final Path modelPath;

    public PmxToolSettingsScreen(Screen parent, Path modelPath) {
        super(Component.translatable("pmx.screen.tool_settings.title"));
        this.parent = parent;
        this.modelPath = modelPath;
    }

    @Override
    protected void init() {
        int rowY = this.height - 32;
        int itemWidth = Math.min(GuiUtil.FOOTER_BUTTON_WIDTH, Math.max(80, this.width / 4));
        int rightX = this.width - 10 - itemWidth;
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.done"), b -> onClose())
                .bounds(rightX, rowY, itemWidth, 20).build());
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        this.renderBackground(graphics);
        graphics.drawCenteredString(this.font, this.title, this.width / 2, 15, 0xFFFFFF);
        super.render(graphics, mouseX, mouseY, partialTick);
    }

    @Override
    public void renderBackground(@NotNull GuiGraphics graphics) {
        GuiUtil.renderDefaultBackground(graphics, this.width, this.height);
    }

    @Override
    public void onClose() {
        if (this.minecraft != null) {
            this.minecraft.setScreen(parent);
        }
    }
}
