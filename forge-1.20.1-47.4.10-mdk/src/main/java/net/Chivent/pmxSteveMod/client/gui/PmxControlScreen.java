package net.Chivent.pmxSteveMod.client.gui;

import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import com.mojang.blaze3d.platform.InputConstants;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;

public class PmxControlScreen extends Screen {
    private final Screen parent;

    public PmxControlScreen(Screen parent) {
        super(Component.literal("PMX Control"));
        this.parent = parent;
    }

    @Override
    protected void init() {
        int centerX = this.width / 2;
        int y = this.height / 4;

        Button toggleButton = Button.builder(toggleLabel(), b -> {
            PmxViewer viewer = PmxViewer.get();
            viewer.setPmxVisible(!viewer.isPmxVisible());
            b.setMessage(toggleLabel());
        }).bounds(centerX - 100, y, 200, 20).build();
        addRenderableWidget(toggleButton);
    }

    private Component toggleLabel() {
        String state = PmxViewer.get().isPmxVisible() ? "ON" : "OFF";
        return Component.literal("Show PMX: " + state);
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
}
