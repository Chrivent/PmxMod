package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Path;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;

public class PmxStateSettingsScreen extends PmxSettingsScreenBase {
    private static final int IDLE_SLOT_INDEX = -2;
    private static final String[] HEADER_KEYS = new String[] {
            "pmx.settings.header.state",
            "pmx.settings.header.motion",
            "pmx.settings.header.loop",
            "pmx.settings.header.camera_anim",
            "pmx.settings.header.camera_lock",
            "pmx.settings.header.music"
    };

    public PmxStateSettingsScreen(Screen parent, Path modelPath) {
        super(parent, modelPath, Component.translatable("pmx.screen.state_settings.title"));
        loadRows();
    }

    @Override
    protected String[] headerKeys() {
        return HEADER_KEYS;
    }

    @Override
    protected void addFooterButtons(int listBottom) {
        int btnWidth = 110;
        addRenderableWidget(Button.builder(
                Component.translatable("pmx.button.add_state"),
                b -> {
                    if (this.minecraft != null) {
                        this.minecraft.setScreen(new PmxAddStateScreen(this));
                    }
                }
        ).bounds(12, listBottom + 6, btnWidth, 20).build());
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.done"), b -> onClose())
                .bounds((this.width - btnWidth) / 2, listBottom + 6, btnWidth, 20).build());
    }

    void addCustomState(String name) {
        if (name == null || name.isBlank()) {
            return;
        }
        SettingsRow row = new SettingsRow(name.trim(), true, -1);
        rows.add(row);
        if (list != null) {
            list.addRow(row);
        }
        markWidthsDirty();
        saveSettings();
    }

    @Override
    protected void loadRows() {
        loadRowsCommon(
                data -> data.slotIndex < 0,
                data -> {
                    String name = data.name;
                    if (!data.custom && data.slotIndex == IDLE_SLOT_INDEX) {
                        name = Component.translatable("pmx.settings.row.idle").getString();
                    }
                    return name;
                },
                () -> {}
        );
        boolean hasIdle = rows.stream().anyMatch(r -> r.slotIndex == IDLE_SLOT_INDEX);
        if (!hasIdle) {
            rows.add(0, new SettingsRow(Component.translatable("pmx.settings.row.idle").getString(), false, IDLE_SLOT_INDEX));
        }
        markWidthsDirty();
    }
}
