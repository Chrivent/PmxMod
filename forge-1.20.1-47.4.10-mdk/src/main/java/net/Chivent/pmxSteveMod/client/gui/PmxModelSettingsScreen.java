package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Path;
import java.util.List;

import net.Chivent.pmxSteveMod.client.settings.PmxModelSettingsStore;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import net.minecraft.client.gui.GuiGraphics;

public class PmxModelSettingsScreen extends PmxSettingsScreenBase {
    private static final int SLOT_COUNT = 6;
    private static final String[] HEADER_KEYS = new String[] {
            "pmx.settings.header.slot",
            "pmx.settings.header.motion",
            "pmx.settings.header.loop",
            "pmx.settings.header.camera_anim",
            "pmx.settings.header.camera_lock",
            "pmx.settings.header.music"
    };

    public PmxModelSettingsScreen(Screen parent, Path modelPath) {
        super(parent, modelPath, Component.translatable("pmx.screen.slot_settings.title"));
        loadRows();
    }

    @Override
    protected void addFooterButtons(int listBottom) {
        int btnWidth = 120;
        addRenderableWidget(Button.builder(
                Component.translatable("pmx.button.state_settings"),
                b -> {
                    if (this.minecraft != null) {
                        this.minecraft.setScreen(new PmxStateSettingsScreen(this, modelPath));
                    }
                }
        ).bounds(12, listBottom + 6, btnWidth, 20).build());
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.done"), b -> onClose())
                .bounds(this.width - 90, this.height - 28, 74, 20).build());
    }

    @Override
    protected String[] headerKeys() {
        return HEADER_KEYS;
    }

    @Override
    protected void loadRows() {
        rows.clear();
        preservedRows.clear();
        List<PmxModelSettingsStore.RowData> saved = store.load(modelPath);
        for (PmxModelSettingsStore.RowData data : saved) {
            if (data.slotIndex >= 0 && data.slotIndex < SLOT_COUNT) {
                String name = Component.translatable("pmx.settings.row.slot", data.slotIndex + 1).getString();
                SettingsRow row = new SettingsRow(name, data.custom, data.slotIndex);
                row.motionLoop = data.motionLoop;
                row.stopOnMove = data.stopOnMove;
                row.cameraLock = data.cameraLock;
                row.motion = data.motion == null ? "" : data.motion;
                row.camera = data.camera == null ? "" : data.camera;
                row.music = data.music == null ? "" : data.music;
                rows.add(row);
            } else {
                preservedRows.add(data);
            }
        }
        if (rows.isEmpty()) {
            for (int i = 1; i <= SLOT_COUNT; i++) {
                rows.add(new SettingsRow(Component.translatable("pmx.settings.row.slot", i).getString(), false, i - 1));
            }
        }
        markWidthsDirty();
    }

    @Override
    protected void renderFirstColumn(GuiGraphics graphics, SettingsRow row, int nameColor, PmxSettingsList.PmxSettingsEntry entry) {
        if (row.slotIndex >= 0) {
            drawSlotIcon(graphics, entry, row.slotIndex);
        } else {
            entry.drawCell(graphics, 0, row.name, nameColor);
        }
    }

    private void drawSlotIcon(GuiGraphics graphics, PmxSettingsList.PmxSettingsEntry entry, int slotIndex) {
        int left = entry.colX[0];
        int width = entry.colW[0];
        int size = Math.min(width, entry.lastHeight) - 6;
        if (size < 8) return;
        int outer = size / 2;
        int inner = Math.max(4, (int) Math.round(outer * 0.5));
        int cx = left + width / 2;
        int cy = entry.lastY + entry.lastHeight / 2;
        int segments = Math.max(24, outer * 2);
        GuiUtil.drawRingSector(graphics, cx, cy, outer, inner, -90.0, 270.0, segments, 0x66000000);
        double step = 360.0 / SLOT_COUNT;
        double centerDeg = -90.0 + (step * slotIndex);
        double startDeg = centerDeg - (step / 2.0);
        double endDeg = centerDeg + (step / 2.0);
        GuiUtil.drawRingSector(graphics, cx, cy, outer, inner, startDeg, endDeg, segments, 0x66FFFFFF);
    }
}
