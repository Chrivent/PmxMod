package net.Chivent.pmxSteveMod.client.gui;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.components.EditBox;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import org.jetbrains.annotations.NotNull;

public class PmxAddStateScreen extends Screen {
    private final PmxStateSettingsScreen parent;
    private EditBox nameBox;

    public PmxAddStateScreen(PmxStateSettingsScreen parent) {
        super(Component.translatable("pmx.screen.add_state.title"));
        this.parent = parent;
    }

    @Override
    protected void init() {
        int centerX = this.width / 2;
        this.nameBox = new EditBox(this.font, centerX - 100, this.height / 2 - 10, 200, 20,
                Component.translatable("pmx.settings.label.state_name"));
        this.nameBox.setMaxLength(32);
        this.addRenderableWidget(this.nameBox);
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.add"), b -> {
            parent.addCustomState(nameBox.getValue());
            Minecraft.getInstance().setScreen(parent);
        }).bounds(centerX - 100, this.height / 2 + 18, 96, 20).build());
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.cancel"), b -> onClose())
                .bounds(centerX + 4, this.height / 2 + 18, 96, 20).build());
        this.setInitialFocus(this.nameBox);
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        this.renderBackground(graphics);
        graphics.drawCenteredString(this.font, this.title, this.width / 2, 40, 0xFFFFFF);
        graphics.drawCenteredString(this.font, Component.translatable("pmx.settings.label.state_name"),
                this.width / 2, this.height / 2 - 26, 0xC0C0C0);
        super.render(graphics, mouseX, mouseY, partialTick);
    }

    @Override
    public void onClose() {
        Minecraft.getInstance().setScreen(parent);
    }
}
