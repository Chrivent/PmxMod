package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.client.Minecraft;
import net.Chivent.pmxSteveMod.client.settings.PmxModelSettingsStore;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.AbstractSelectionList;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import org.jetbrains.annotations.NotNull;

public class PmxModelSettingsScreen extends Screen {
    private static final int HEADER_HEIGHT = 16;
    private static final int LIST_TOP = 46;
    private static final int LIST_BOTTOM_PAD = 34;
    private static final int LIST_ITEM_HEIGHT = 22;
    private static final int LIST_SIDE_PAD = 20;
    private static final int COLUMN_COUNT = 7;
    private static final int SLOT_COUNT = 6;
    private static final String[] HEADER_KEYS = new String[] {
            "pmx.settings.header.state",
            "pmx.settings.header.motion",
            "pmx.settings.header.loop",
            "pmx.settings.header.stop_on_move",
            "pmx.settings.header.camera_anim",
            "pmx.settings.header.camera_lock",
            "pmx.settings.header.music"
    };

    private final Screen parent;
    private final Path modelPath;
    private final List<SettingsRow> rows = new ArrayList<>();
    private final PmxModelSettingsStore store = PmxModelSettingsStore.get();
    private PmxSettingsList list;

    public PmxModelSettingsScreen(Screen parent, Path modelPath) {
        super(Component.translatable("pmx.screen.model_settings.title"));
        this.parent = parent;
        this.modelPath = modelPath;
        loadRows();
    }

    @Override
    protected void init() {
        int listBottom = this.height - LIST_BOTTOM_PAD;
        int listHeight = listBottom - LIST_TOP;
        this.list = new PmxSettingsList(this, minecraft, this.width - (LIST_SIDE_PAD * 2),
                listHeight, LIST_TOP, listBottom, LIST_ITEM_HEIGHT);
        this.list.setLeftPos(LIST_SIDE_PAD);
        this.list.setRenderBackground(false);
        this.list.setRenderTopAndBottom(false);
        addWidget(this.list);
        for (SettingsRow row : rows) {
            this.list.addRow(row);
        }
        int btnWidth = 110;
        addRenderableWidget(Button.builder(
                Component.translatable("pmx.button.add_state"),
                b -> {
                    if (this.minecraft != null) {
                        this.minecraft.setScreen(new PmxAddStateScreen(this));
                    }
                }
        ).bounds((this.width - btnWidth) / 2, listBottom + 6, btnWidth, 20).build());
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.done"), b -> onClose())
                .bounds(this.width - 90, this.height - 28, 74, 20).build());
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        this.renderBackground(graphics);
        graphics.drawCenteredString(this.font, this.title, this.width / 2, 16, 0xFFFFFF);
        renderHeader(graphics);
        if (list != null) {
            list.render(graphics, mouseX, mouseY, partialTick);
        }
        super.render(graphics, mouseX, mouseY, partialTick);
    }

    @Override
    public void renderBackground(@NotNull GuiGraphics graphics) {
        GuiUtil.renderDefaultBackground(graphics, this.width, this.height);
    }

    @Override
    public void onClose() {
        Minecraft.getInstance().setScreen(parent);
    }

    @Override
    public boolean keyPressed(int keyCode, int scanCode, int modifiers) {
        if (keyCode == org.lwjgl.glfw.GLFW.GLFW_KEY_ESCAPE) {
            onClose();
            return true;
        }
        return super.keyPressed(keyCode, scanCode, modifiers);
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
        saveSettings();
    }

    private void loadRows() {
        rows.clear();
        List<PmxModelSettingsStore.RowData> saved = store.load(modelPath);
        if (saved.isEmpty()) {
            initDefaultRows();
            return;
        }
        for (PmxModelSettingsStore.RowData data : saved) {
            String name = data.name;
            if (!data.custom && data.slotIndex >= 0) {
                name = Component.translatable("pmx.settings.row.slot", data.slotIndex + 1).getString();
            }
            SettingsRow row = new SettingsRow(name, data.custom, data.slotIndex);
            row.motionLoop = data.motionLoop;
            row.stopOnMove = data.stopOnMove;
            row.cameraLock = data.cameraLock;
            row.motion = data.motion == null ? "" : data.motion;
            row.camera = data.camera == null ? "" : data.camera;
            row.music = data.music == null ? "" : data.music;
            rows.add(row);
        }
    }

    private void saveSettings() {
        List<PmxModelSettingsStore.RowData> out = new ArrayList<>();
        for (SettingsRow row : rows) {
            PmxModelSettingsStore.RowData data = new PmxModelSettingsStore.RowData();
            data.name = row.name;
            data.custom = row.custom;
            data.slotIndex = row.slotIndex;
            data.motionLoop = row.motionLoop;
            data.stopOnMove = row.stopOnMove;
            data.cameraLock = row.cameraLock;
            data.motion = row.motion;
            data.camera = row.camera;
            data.music = row.music;
            out.add(data);
        }
        store.save(modelPath, out);
    }

    private void openMotionPicker(SettingsRow row) {
        PmxFileSelectScreen screen = new PmxFileSelectScreen(
                this,
                net.Chivent.pmxSteveMod.viewer.PmxViewer.get().getUserMotionDir(),
                new String[] {".vmd"},
                Component.translatable("pmx.screen.select_motion.title"),
                Component.translatable("pmx.button.open_motion_folder"),
                path -> {
                    row.motion = path == null ? "" : path.getFileName().toString();
                    saveSettings();
                }
        );
        if (this.minecraft != null) {
            this.minecraft.setScreen(screen);
        }
    }

    private void openCameraPicker(SettingsRow row) {
        PmxFileSelectScreen screen = new PmxFileSelectScreen(
                this,
                net.Chivent.pmxSteveMod.viewer.PmxViewer.get().getUserCameraDir(),
                new String[] {".vmd"},
                Component.translatable("pmx.screen.select_camera.title"),
                Component.translatable("pmx.button.open_camera_folder"),
                path -> {
                    row.camera = path == null ? "" : path.getFileName().toString();
                    saveSettings();
                }
        );
        if (this.minecraft != null) {
            this.minecraft.setScreen(screen);
        }
    }

    private void openMusicPicker(SettingsRow row) {
        PmxFileSelectScreen screen = new PmxFileSelectScreen(
                this,
                net.Chivent.pmxSteveMod.viewer.PmxViewer.get().getUserMusicDir(),
                new String[] {".ogg", ".mp3", ".wav"},
                Component.translatable("pmx.screen.select_music.title"),
                Component.translatable("pmx.button.open_music_folder"),
                path -> {
                    row.music = path == null ? "" : path.getFileName().toString();
                    saveSettings();
                }
        );
        if (this.minecraft != null) {
            this.minecraft.setScreen(screen);
        }
    }

    private void initDefaultRows() {
        for (int i = 1; i <= 6; i++) {
            rows.add(new SettingsRow(Component.translatable("pmx.settings.row.slot", i).getString(), false, i - 1));
        }
    }

    private void renderHeader(GuiGraphics graphics) {
        if (list == null) return;
        int x = list.getListLeft();
        int width = list.getListWidth();
        int y = LIST_TOP - HEADER_HEIGHT;
        int[] colWidths = getColumnWidths(width);
        int curX = x;
        for (int i = 0; i < COLUMN_COUNT; i++) {
            Component label = Component.translatable(HEADER_KEYS[i]);
            int textWidth = this.font.width(label);
            int drawX = curX + Math.max(2, (colWidths[i] - textWidth) / 2);
            graphics.drawString(this.font, label, drawX, y, 0xB0B0B0, false);
            curX += colWidths[i];
        }
    }

    private int[] getColumnWidths(int totalWidth) {
        int[] widths = new int[COLUMN_COUNT];
        int padding = 10;
        int sum = 0;
        for (int i = 0; i < COLUMN_COUNT; i++) {
            int min = this.font.width(Component.translatable(HEADER_KEYS[i])) + padding;
            widths[i] = Math.max(24, min);
            sum += widths[i];
        }
        if (sum > totalWidth) {
            float scale = totalWidth / (float) sum;
            int scaledSum = 0;
            for (int i = 0; i < COLUMN_COUNT; i++) {
                widths[i] = Math.max(24, Math.round(widths[i] * scale));
                scaledSum += widths[i];
            }
            widths[COLUMN_COUNT - 1] += totalWidth - scaledSum;
            return widths;
        }
        int extra = totalWidth - sum;
        int per = extra / COLUMN_COUNT;
        int rem = extra % COLUMN_COUNT;
        for (int i = 0; i < COLUMN_COUNT; i++) {
            widths[i] += per + (i < rem ? 1 : 0);
        }
        return widths;
    }

    private static final class SettingsRow {
        private final String name;
        private final boolean custom;
        private final int slotIndex;
        private boolean motionLoop;
        private boolean stopOnMove;
        private boolean cameraLock;
        private String motion = "";
        private String camera = "";
        private String music = "";

        private SettingsRow(String name, boolean custom, int slotIndex) {
            this.name = name;
            this.custom = custom;
            this.slotIndex = slotIndex;
        }
    }

    private static final class PmxSettingsList extends AbstractSelectionList<PmxSettingsList.PmxSettingsEntry> {
        private final PmxModelSettingsScreen screen;

        public PmxSettingsList(PmxModelSettingsScreen screen, Minecraft minecraft, int width, int height,
                               int y0, int y1, int itemHeight) {
            super(minecraft, width, height, y0, y1, itemHeight);
            this.screen = screen;
            this.setRenderHeader(false, 0);
        }

        public void addRow(SettingsRow row) {
            addEntry(new PmxSettingsEntry(row));
        }

        public int getListLeft() {
            return this.getRowLeft();
        }

        public int getListWidth() {
            return this.getRowWidth();
        }

        @Override
        public void updateNarration(@NotNull net.minecraft.client.gui.narration.NarrationElementOutput output) {
            // No narration for now.
        }

        @Override
        protected int getScrollbarPosition() {
            return this.getRowRight() + 6;
        }

        @Override
        protected void renderSelection(@NotNull GuiGraphics graphics, int y, int entryWidth, int entryHeight,
                                       int borderColor, int fillColor) {
            // Disable selection highlight.
        }

        private final class PmxSettingsEntry extends AbstractSelectionList.Entry<PmxSettingsEntry> {
            private final SettingsRow row;
            private final int[] colX = new int[COLUMN_COUNT];
            private final int[] colW = new int[COLUMN_COUNT];
            private int lastX;
            private int lastY;
            private int lastHeight;

            private PmxSettingsEntry(SettingsRow row) {
                this.row = row;
            }

            @Override
            public void render(@NotNull GuiGraphics graphics, int index, int y, int x, int width, int height,
                               int mouseX, int mouseY, boolean hovered, float partialTick) {
                lastX = x;
                lastY = y;
                lastHeight = height;
                int[] widths = screen.getColumnWidths(width);
                int curX = x;
                for (int i = 0; i < COLUMN_COUNT; i++) {
                    colX[i] = curX;
                    colW[i] = widths[i];
                    curX += widths[i];
                }
                int nameColor = row.custom ? 0xE8E8E8 : 0xD0D0D0;
                if (row.slotIndex >= 0) {
                    drawSlotIcon(graphics, row.slotIndex);
                } else {
                    drawCell(graphics, 0, row.name, nameColor);
                }
                drawCell(graphics, 1, row.motion.isBlank()
                        ? Component.translatable("pmx.settings.value.unset").getString()
                        : row.motion, 0xB0B0B0);
                drawCheckbox(graphics, 2, row.motionLoop);
                drawCheckbox(graphics, 3, row.stopOnMove);
                drawCell(graphics, 4, row.camera.isBlank()
                        ? Component.translatable("pmx.settings.value.unset").getString()
                        : row.camera, 0xB0B0B0);
                drawCheckbox(graphics, 5, row.cameraLock);
                drawCell(graphics, 6, row.music.isBlank()
                        ? Component.translatable("pmx.settings.value.unset").getString()
                        : row.music, 0xB0B0B0);
                drawColumnLines(graphics);
            }

            @Override
            public boolean mouseClicked(double mouseX, double mouseY, int button) {
                if (mouseY < lastY || mouseY > lastY + lastHeight) return false;
                for (int i = 0; i < COLUMN_COUNT; i++) {
                    int left = colX[i];
                    int right = left + colW[i];
                    if (mouseX < left || mouseX > right) continue;
                if (screen.list != null) {
                    screen.list.setSelected(this);
                }
                switch (i) {
                    case 2 -> row.motionLoop = !row.motionLoop;
                    case 3 -> row.stopOnMove = !row.stopOnMove;
                    case 5 -> row.cameraLock = !row.cameraLock;
                    case 1 -> screen.openMotionPicker(row);
                    case 4 -> screen.openCameraPicker(row);
                    case 6 -> screen.openMusicPicker(row);
                    default -> {
                    }
                }
                if (i == 2 || i == 3 || i == 5) {
                    screen.saveSettings();
                }
                return true;
            }
                return false;
            }

            private void drawCell(GuiGraphics graphics, int col, String text, int color) {
                int left = colX[col];
                int width = colW[col];
                int textWidth = screen.font.width(text);
                int drawX = left + Math.max(2, (width - textWidth) / 2);
                graphics.drawString(screen.font, text, drawX, lastY + 6, color, false);
            }

            private void drawCheckbox(GuiGraphics graphics, int col, boolean checked) {
                int left = colX[col];
                int width = colW[col];
                int size = 10;
                int cx = left + (width - size) / 2;
                int cy = lastY + (lastHeight - size) / 2;
                graphics.fill(cx, cy, cx + size, cy + size, 0xFF2A2A2A);
                graphics.fill(cx + 1, cy + 1, cx + size - 1, cy + size - 1, 0xFF0E0E0E);
                if (checked) {
                    graphics.fill(cx + 2, cy + 2, cx + size - 2, cy + size - 2, 0xFFB0B0B0);
                }
            }

            private void drawColumnLines(GuiGraphics graphics) {
                int x = lastX;
                for (int i = 0; i < COLUMN_COUNT - 1; i++) {
                    x += colW[i];
                    graphics.fill(x, lastY, x + 1, lastY + lastHeight, 0x66FFFFFF);
                }
            }

            private void drawSlotIcon(GuiGraphics graphics, int slotIndex) {
                int left = colX[0];
                int width = colW[0];
                int size = Math.min(width, lastHeight) - 6;
                if (size < 8) return;
                int outer = size / 2;
                int inner = Math.max(4, (int) Math.round(outer * 0.5));
                int cx = left + width / 2;
                int cy = lastY + lastHeight / 2;
                int segments = Math.max(24, outer * 2);
                GuiUtil.drawRingSector(graphics, cx, cy, outer, inner, -90.0, 270.0, segments, 0x66000000);
                double step = 360.0 / SLOT_COUNT;
                double centerDeg = -90.0 + (step * slotIndex);
                double startDeg = centerDeg - (step / 2.0);
                double endDeg = centerDeg + (step / 2.0);
                GuiUtil.drawRingSector(graphics, cx, cy, outer, inner, startDeg, endDeg, segments, 0x66FFFFFF);
            }
        }
    }
}
