package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Path;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import net.minecraft.client.gui.GuiGraphics;

public class PmxSlotSettingsScreen extends PmxSettingsScreenBase {
    private static final int SLOT_COUNT = PmxEmoteWheelScreen.SLOT_COUNT;
    private static final String[] HEADER_KEYS = new String[] {
            "pmx.settings.header.slot",
            "pmx.settings.header.motion",
            "pmx.settings.header.loop",
            "pmx.settings.header.camera_anim",
            "pmx.settings.header.camera_lock",
            "pmx.settings.header.music"
    };

    public PmxSlotSettingsScreen(Screen parent, Path modelPath) {
        super(parent, modelPath, Component.translatable("pmx.screen.slot_settings.title"));
        loadRows();
    }

    @Override
    protected void addFooterButtons(int listBottom) {
        int maxWidth = this.width - 24;
        int btnWidth = Math.min(GuiUtil.FOOTER_BUTTON_WIDTH, Math.max(80, maxWidth));
        int leftX = (this.width - btnWidth) / 2;
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.done"), b -> onClose())
                .bounds(leftX, listBottom + 6, btnWidth, 20).build());
    }

    @Override
    protected String[] headerKeys() {
        return HEADER_KEYS;
    }

    @Override
    protected void loadRows() {
        loadRowsCommon(
                data -> data.slotIndex >= 0 && data.slotIndex < SLOT_COUNT,
                data -> Component.translatable("pmx.settings.row.slot", data.slotIndex + 1).getString(),
                () -> {
            for (int i = 1; i <= SLOT_COUNT; i++) {
                rows.add(new SettingsRow(Component.translatable("pmx.settings.row.slot", i).getString(), false, i - 1));
            }
                }
        );
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
        int inner = GuiUtil.getWheelDeadZoneRadius(outer);
        int cx = left + width / 2;
        int cy = entry.lastY + entry.lastHeight / 2;
        GuiUtil.drawWheelRing(graphics, cx, cy, outer, inner, 0x99000000);
        GuiUtil.drawWheelSelection(graphics, cx, cy, outer, inner, slotIndex, SLOT_COUNT, 0x55FFFFFF);
    }
}
