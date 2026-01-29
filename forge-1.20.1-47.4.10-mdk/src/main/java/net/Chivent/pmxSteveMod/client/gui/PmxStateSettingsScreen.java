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
    private Button removeButton;

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
        removeButton = addRenderableWidget(Button.builder(
                Component.translatable("pmx.button.remove_state"),
                b -> removeSelectedState()
        ).bounds(12 + btnWidth + 6, listBottom + 6, btnWidth, 20).build());
        updateRemoveButtonState();
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
        saveSettings(row);
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
        for (SettingsRow row : rows) {
            if (!row.custom && row.slotIndex == IDLE_SLOT_INDEX) {
                row.motionLoop = true;
            }
        }
        boolean hasIdle = rows.stream().anyMatch(r -> r.slotIndex == IDLE_SLOT_INDEX);
        if (!hasIdle) {
            SettingsRow row = new SettingsRow(Component.translatable("pmx.settings.row.idle").getString(), false, IDLE_SLOT_INDEX);
            row.motionLoop = true;
            rows.add(0, row);
        }
        markWidthsDirty();
    }

    @Override
    protected boolean allowMotionLoopToggle(SettingsRow row) {
        return row.custom || row.slotIndex != IDLE_SLOT_INDEX;
    }

    @Override
    protected boolean isSelectableRow(SettingsRow row) {
        return row.custom || row.slotIndex != IDLE_SLOT_INDEX;
    }

    @Override
    protected void onSelectionChanged() {
        updateRemoveButtonState();
    }

    private void removeSelectedState() {
        if (selectedRow == null) return;
        if (!selectedRow.custom || selectedRow.slotIndex == IDLE_SLOT_INDEX) return;
        rows.remove(selectedRow);
        if (list != null) {
            list.removeRow(selectedRow);
        }
        selectedRow = null;
        updateRemoveButtonState();
        markWidthsDirty();
        saveSettings(null);
    }

    private void updateRemoveButtonState() {
        if (removeButton == null) return;
        removeButton.active = selectedRow != null
                && selectedRow.custom
                && selectedRow.slotIndex != IDLE_SLOT_INDEX;
    }

    @Override
    protected boolean useSelectionBorder() {
        return true;
    }

    @Override
    protected void saveSettings(SettingsRow changedRow) {
        net.Chivent.pmxSteveMod.client.settings.PmxModelSettingsStore.RowData prevIdle = null;
        if (changedRow != null && changedRow.slotIndex == IDLE_SLOT_INDEX) {
            prevIdle = store.getIdleRow(modelPath);
        }
        for (SettingsRow row : rows) {
            if (!row.custom && row.slotIndex == IDLE_SLOT_INDEX) {
                row.motionLoop = true;
            }
        }
        super.saveSettings(changedRow);
        if (changedRow != null && changedRow.slotIndex == IDLE_SLOT_INDEX) {
            net.Chivent.pmxSteveMod.viewer.PmxViewer viewer = net.Chivent.pmxSteveMod.viewer.PmxViewer.get();
            if (!viewer.isPmxVisible()) return;
            net.Chivent.pmxSteveMod.viewer.PmxInstance instance = viewer.instance();
            java.nio.file.Path currentMotion = instance.getCurrentMotionPath();
            if (currentMotion == null || matchesMotionPath(viewer, currentMotion, prevIdle)) {
                if (changedRow.motion == null || changedRow.motion.isBlank()) {
                    instance.resetToDefaultPose();
                } else {
                    viewer.playIdleOrDefault();
                }
            }
        }
    }

    private boolean matchesMotionPath(net.Chivent.pmxSteveMod.viewer.PmxViewer viewer,
                                      java.nio.file.Path currentMotion,
                                      net.Chivent.pmxSteveMod.client.settings.PmxModelSettingsStore.RowData row) {
        if (row == null || row.motion == null || row.motion.isBlank()) return false;
        java.nio.file.Path resolved = viewer.getUserMotionDir().resolve(row.motion);
        try {
            resolved = resolved.toAbsolutePath().normalize();
        } catch (Exception ignored) {
        }
        return resolved.equals(currentMotion);
    }
}
