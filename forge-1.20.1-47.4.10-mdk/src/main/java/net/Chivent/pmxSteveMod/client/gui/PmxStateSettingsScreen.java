package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Path;
import java.util.List;

import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;

public class PmxStateSettingsScreen extends PmxSettingsScreenBase {
    private static final int IDLE_SLOT_INDEX = -2;
    private static final int CROUCHING_IDLE_INDEX = -3;
    private static final int SWIMMING_IDLE_INDEX = -4;
    private static final int FALL_FLYING_IDLE_INDEX = -5;
    private static final int SLEEPING_IDLE_INDEX = -6;
    private static final int DYING_IDLE_INDEX = -7;
    private static final int SITTING_IDLE_INDEX = -8;
    private static final int[] FIXED_IDLE_SLOTS = new int[] {
            IDLE_SLOT_INDEX,
            CROUCHING_IDLE_INDEX,
            SWIMMING_IDLE_INDEX,
            FALL_FLYING_IDLE_INDEX,
            SLEEPING_IDLE_INDEX,
            DYING_IDLE_INDEX,
            SITTING_IDLE_INDEX
    };
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
        int gap = 6;
        int rowY = listBottom + 6;
        int avail = this.width - 24;
        int btnWidth = Math.min(GuiUtil.FOOTER_BUTTON_WIDTH, Math.max(60, (avail - gap * 2) / 3));
        int totalRowWidth = btnWidth * 3 + gap * 2;
        int rowX = (this.width - totalRowWidth) / 2;
        int removeX = rowX + btnWidth + gap;
        int doneX = rowX + btnWidth * 2 + gap * 2;
        addRenderableWidget(Button.builder(
                Component.translatable("pmx.button.add_state"),
                b -> {
                    if (this.minecraft != null) {
                        this.minecraft.setScreen(new PmxAddStateScreen(this));
                    }
                }
        ).bounds(rowX, rowY, btnWidth, 20).build());
        removeButton = addRenderableWidget(Button.builder(
                Component.translatable("pmx.button.remove_state"),
                b -> removeSelectedState()
        ).bounds(removeX, rowY, btnWidth, 20).build());
        updateRemoveButtonState();
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.done"), b -> onClose())
                .bounds(doneX, rowY, btnWidth, 20).build());
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
                    if (!data.custom) {
                        String labelKey = labelKeyForSlot(data.slotIndex);
                        if (labelKey != null) {
                            return Component.translatable(labelKey).getString();
                        }
                    }
                    return data.name;
                },
                () -> {}
        );
        normalizeFixedIdleRows();
        markWidthsDirty();
    }

    @Override
    protected boolean allowMotionLoopToggle(SettingsRow row) {
        return row.custom || !isFixedIdleSlot(row.slotIndex);
    }

    @Override
    protected boolean isSelectableRow(SettingsRow row) {
        return row.custom || !isFixedIdleSlot(row.slotIndex);
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
            if (!row.custom && isFixedIdleSlot(row.slotIndex)) {
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

    private void normalizeFixedIdleRows() {
        java.util.Map<Integer, SettingsRow> fixed = new java.util.HashMap<>();
        List<SettingsRow> others = new java.util.ArrayList<>();
        for (SettingsRow row : rows) {
            if (!row.custom && isFixedIdleSlot(row.slotIndex)) {
                fixed.put(row.slotIndex, row);
            } else {
                others.add(row);
            }
        }
        rows.clear();
        for (int slot : FIXED_IDLE_SLOTS) {
            SettingsRow row = fixed.get(slot);
            if (row == null) {
                String labelKey = labelKeyForSlot(slot);
                String name = labelKey == null ? "" : Component.translatable(labelKey).getString();
                row = new SettingsRow(name, false, slot);
            }
            row.motionLoop = true;
            rows.add(row);
        }
        rows.addAll(others);
    }

    private boolean isFixedIdleSlot(int slotIndex) {
        for (int slot : FIXED_IDLE_SLOTS) {
            if (slot == slotIndex) return true;
        }
        return false;
    }

    private String labelKeyForSlot(int slotIndex) {
        return switch (slotIndex) {
            case IDLE_SLOT_INDEX -> "pmx.settings.row.idle";
            case CROUCHING_IDLE_INDEX -> "pmx.pose.crouching";
            case SWIMMING_IDLE_INDEX -> "pmx.pose.swimming";
            case FALL_FLYING_IDLE_INDEX -> "pmx.pose.fall_flying";
            case SLEEPING_IDLE_INDEX -> "pmx.pose.sleeping";
            case DYING_IDLE_INDEX -> "pmx.pose.dying";
            case SITTING_IDLE_INDEX -> "pmx.pose.sitting";
            default -> null;
        };
    }
}
