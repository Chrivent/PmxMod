package net.Chivent.pmxSteveMod.client.gui;

import com.mojang.blaze3d.platform.InputConstants;
import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraft.Util;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import org.jetbrains.annotations.NotNull;

import java.nio.file.Path;

public class PmxControlScreen extends Screen {
    private final Screen parent;
    private Path modelDir;

    public PmxControlScreen(Screen parent) {
        super(Component.translatable("pmx.screen.control.title"));
        this.parent = parent;
    }

    @Override
    protected void init() {
        int centerX = this.width / 2;
        int y = this.height / 4;

        PmxViewer viewer = PmxViewer.get();
        modelDir = viewer.getUserModelDir();
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.select_model"), b -> {
            if (this.minecraft != null) {
                this.minecraft.setScreen(new PmxModelSelectScreen(this));
            }
        }).bounds(centerX - 100, y, 200, 20).build());

        Button toggleButton = Button.builder(toggleLabel(), b -> {
            viewer.setPmxVisible(!viewer.isPmxVisible());
            b.setMessage(toggleLabel());
        }).bounds(centerX - 100, y + 30, 200, 20).build();
        addRenderableWidget(toggleButton);

        addRenderableWidget(Button.builder(Component.translatable("pmx.button.open_folder"), b -> {
            if (modelDir != null) {
                Util.getPlatform().openFile(modelDir.toFile());
            }
        }).bounds(centerX - 100, y + 60, 200, 20).build());
    }

    private Component toggleLabel() {
        String state = PmxViewer.get().isPmxVisible()
                ? Component.translatable("pmx.state.on").getString()
                : Component.translatable("pmx.state.off").getString();
        return Component.translatable("pmx.button.toggle", state);
    }

    @Override
    public boolean isPauseScreen() {
        return false;
    }

    @Override
    public boolean keyPressed(int keyCode, int scanCode, int modifiers) {
        if (PmxKeyMappings.OPEN_MENU.isActiveAndMatches(InputConstants.getKey(keyCode, scanCode))) {
            if (this.minecraft != null) {
                this.minecraft.setScreen(parent);
            }
            return true;
        }
        return super.keyPressed(keyCode, scanCode, modifiers);
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        super.render(graphics, mouseX, mouseY, partialTick);
        PmxViewer viewer = PmxViewer.get();
        Path sel = viewer.getSelectedModelPath();
        String label = sel != null ? sel.getFileName().toString()
                : Component.translatable("pmx.state.none").getString();
        graphics.drawString(this.font,
                Component.translatable("pmx.screen.control.selected", label),
                10, 10, 0xFFFFFF, false);
        if (modelDir != null) {
            graphics.drawString(this.font,
                    Component.translatable("pmx.screen.control.folder", modelDir),
                    10, 24, 0xA0A0A0, false);
        }
        graphics.drawString(this.font,
                Component.translatable("pmx.screen.control.hint_close"),
                10, 38, 0xA0A0A0, false);
    }
}
