package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;
import java.util.function.Predicate;
import net.Chivent.pmxSteveMod.client.settings.PmxModelSettingsStore;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.AbstractSelectionList;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import org.jetbrains.annotations.NotNull;

public abstract class PmxSettingsScreenBase extends Screen {
    protected static final int HEADER_HEIGHT = 16;
    protected static final int LIST_TOP = 56;
    protected static final int LIST_BOTTOM_PAD = 34;
    protected static final int LIST_ITEM_HEIGHT = 22;
    protected static final int COLUMN_COUNT = 6;

    protected final Screen parent;
    protected final Path modelPath;
    protected final List<SettingsRow> rows = new ArrayList<>();
    protected final List<PmxModelSettingsStore.RowData> preservedRows = new ArrayList<>();
    protected final PmxModelSettingsStore store = PmxModelSettingsStore.get();
    protected PmxSettingsList list;
    protected int[] cachedColumnWidths;
    protected int cachedColumnTotalWidth = -1;

    protected PmxSettingsScreenBase(Screen parent, Path modelPath, Component title) {
        super(title);
        this.parent = parent;
        this.modelPath = modelPath;
    }

    protected abstract String[] headerKeys();

    protected abstract void loadRows();

    protected abstract void addFooterButtons(int listBottom);

    protected void renderFirstColumn(GuiGraphics graphics, SettingsRow row, int nameColor, PmxSettingsList.PmxSettingsEntry entry) {
        entry.drawCell(graphics, 0, row.name, nameColor);
    }

    protected boolean allowMotionLoopToggle(SettingsRow row) {
        return true;
    }

    @Override
    protected void init() {
        int listBottom = this.height - LIST_BOTTOM_PAD;
        int listHeight = listBottom - LIST_TOP;
        this.list = new PmxSettingsList(this, minecraft, this.width,
                listHeight, LIST_TOP, listBottom, LIST_ITEM_HEIGHT);
        this.list.setLeftPos(0);
        this.list.setRenderBackground(false);
        this.list.setRenderTopAndBottom(false);
        addWidget(this.list);
        for (SettingsRow row : rows) {
            this.list.addRow(row);
        }
        addFooterButtons(listBottom);
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
        super.renderBackground(graphics);
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

    protected void saveSettings() {
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
        out.addAll(preservedRows);
        store.save(modelPath, out);
    }

    protected void openMotionPicker(SettingsRow row) {
        final PmxFileSelectScreen[] screenRef = new PmxFileSelectScreen[1];
        PmxFileSelectScreen screen = new PmxFileSelectScreen(
                this,
                net.Chivent.pmxSteveMod.viewer.PmxViewer.get().getUserMotionDir(),
                new String[] {".vmd"},
                Component.translatable("pmx.screen.select_motion.title"),
                Component.translatable("pmx.button.open_motion_folder"),
                path -> {
                    if (path == null) {
                        if (screenRef[0] == null) return;
                        if (screenRef[0].isExplicitNoneSelected()) {
                            row.motion = "";
                        } else {
                            return;
                        }
                    } else {
                        row.motion = path.getFileName().toString();
                    }
                    markWidthsDirty();
                    saveSettings();
                }
        );
        screenRef[0] = screen;
        if (this.minecraft != null) {
            this.minecraft.setScreen(screen);
        }
    }

    protected void openCameraPicker(SettingsRow row) {
        final PmxFileSelectScreen[] screenRef = new PmxFileSelectScreen[1];
        PmxFileSelectScreen screen = new PmxFileSelectScreen(
                this,
                net.Chivent.pmxSteveMod.viewer.PmxViewer.get().getUserCameraDir(),
                new String[] {".vmd"},
                Component.translatable("pmx.screen.select_camera.title"),
                Component.translatable("pmx.button.open_camera_folder"),
                path -> {
                    if (path == null) {
                        if (screenRef[0] == null) return;
                        if (screenRef[0].isExplicitNoneSelected()) {
                            row.camera = "";
                        } else {
                            return;
                        }
                    } else {
                        row.camera = path.getFileName().toString();
                    }
                    markWidthsDirty();
                    saveSettings();
                }
        );
        screenRef[0] = screen;
        if (this.minecraft != null) {
            this.minecraft.setScreen(screen);
        }
    }

    protected void openMusicPicker(SettingsRow row) {
        final PmxFileSelectScreen[] screenRef = new PmxFileSelectScreen[1];
        PmxFileSelectScreen screen = new PmxFileSelectScreen(
                this,
                net.Chivent.pmxSteveMod.viewer.PmxViewer.get().getUserMusicDir(),
                new String[] {".ogg", ".mp3", ".wav"},
                Component.translatable("pmx.screen.select_music.title"),
                Component.translatable("pmx.button.open_music_folder"),
                path -> {
                    if (path == null) {
                        if (screenRef[0] == null) return;
                        if (screenRef[0].isExplicitNoneSelected()) {
                            row.music = "";
                        } else {
                            return;
                        }
                    } else {
                        row.music = path.getFileName().toString();
                    }
                    markWidthsDirty();
                    saveSettings();
                }
        );
        screenRef[0] = screen;
        if (this.minecraft != null) {
            this.minecraft.setScreen(screen);
        }
    }

    protected void loadRowsCommon(
            Predicate<PmxModelSettingsStore.RowData> includeRow,
            Function<PmxModelSettingsStore.RowData, String> nameResolver,
            Runnable addDefaults) {
        rows.clear();
        preservedRows.clear();
        List<PmxModelSettingsStore.RowData> saved = store.load(modelPath);
        for (PmxModelSettingsStore.RowData data : saved) {
            if (includeRow.test(data)) {
                String name = nameResolver.apply(data);
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
            addDefaults.run();
        }
        markWidthsDirty();
    }

    private void renderHeader(GuiGraphics graphics) {
        if (list == null) return;
        int x = list.getListLeft();
        int width = list.getListWidth();
        int y = LIST_TOP - HEADER_HEIGHT;
        int[] colWidths = getColumnWidths(width);
        int curX = x;
        String[] headers = headerKeys();
        for (int i = 0; i < COLUMN_COUNT; i++) {
            Component label = Component.translatable(headers[i]);
            int textWidth = this.font.width(label);
            int drawX = curX + Math.max(2, (colWidths[i] - textWidth) / 2);
            graphics.drawString(this.font, label, drawX, y, 0xF0F0F0, false);
            curX += colWidths[i];
        }
    }

    protected int[] getColumnWidths(int totalWidth) {
        if (cachedColumnWidths != null && cachedColumnTotalWidth == totalWidth) {
            return cachedColumnWidths;
        }
        int[] widths = new int[COLUMN_COUNT];
        int padding = 10;
        int sum = 0;
        String[] headers = headerKeys();
        for (int i = 0; i < COLUMN_COUNT; i++) {
            int min = this.font.width(Component.translatable(headers[i])) + padding;
            widths[i] = Math.max(24, min);
            sum += widths[i];
        }
        widths[0] = Math.max(widths[0], maxRowWidth(0) + padding);
        widths[1] = Math.max(widths[1], maxRowWidth(1) + padding);
        widths[3] = Math.max(widths[3], maxRowWidth(3) + padding);
        widths[5] = Math.max(widths[5], maxRowWidth(5) + padding);
        sum = 0;
        for (int w : widths) sum += w;
        if (sum > totalWidth) {
            float scale = totalWidth / (float) sum;
            int scaledSum = 0;
            for (int i = 0; i < COLUMN_COUNT; i++) {
                widths[i] = Math.max(24, Math.round(widths[i] * scale));
                scaledSum += widths[i];
            }
            widths[COLUMN_COUNT - 1] += totalWidth - scaledSum;
            cachedColumnWidths = widths;
            cachedColumnTotalWidth = totalWidth;
            return cachedColumnWidths;
        }
        int extra = totalWidth - sum;
        int per = extra / COLUMN_COUNT;
        int rem = extra % COLUMN_COUNT;
        for (int i = 0; i < COLUMN_COUNT; i++) {
            widths[i] += per + (i < rem ? 1 : 0);
        }
        cachedColumnWidths = widths;
        cachedColumnTotalWidth = totalWidth;
        return cachedColumnWidths;
    }

    private int maxRowWidth(int col) {
        int max = 0;
        String unset = Component.translatable("pmx.settings.value.unset").getString();
        for (SettingsRow row : rows) {
            String value = switch (col) {
                case 0 -> row.custom ? row.name : "";
                case 1 -> row.motion.isBlank() ? unset : row.motion;
                case 3 -> row.camera.isBlank() ? unset : row.camera;
                case 5 -> row.music.isBlank() ? unset : row.music;
                default -> "";
            };
            if (!value.isBlank()) {
                max = Math.max(max, this.font.width(value));
            }
        }
        return max;
    }

    protected void markWidthsDirty() {
        cachedColumnWidths = null;
        cachedColumnTotalWidth = -1;
    }

    protected static final class SettingsRow {
        protected final String name;
        protected final boolean custom;
        protected final int slotIndex;
        protected boolean motionLoop;
        protected boolean stopOnMove;
        protected boolean cameraLock;
        protected String motion = "";
        protected String camera = "";
        protected String music = "";

        protected SettingsRow(String name, boolean custom, int slotIndex) {
            this.name = name;
            this.custom = custom;
            this.slotIndex = slotIndex;
        }
    }

    protected static final class PmxSettingsList extends AbstractSelectionList<PmxSettingsList.PmxSettingsEntry> {
        private final PmxSettingsScreenBase screen;

        public PmxSettingsList(PmxSettingsScreenBase screen, Minecraft minecraft, int width, int height,
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
            return this.width;
        }

        @Override
        public int getRowWidth() {
            return this.width;
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

        protected class PmxSettingsEntry extends AbstractSelectionList.Entry<PmxSettingsEntry> {
            private final SettingsRow row;
            protected final int[] colX = new int[COLUMN_COUNT];
            protected final int[] colW = new int[COLUMN_COUNT];
            protected int lastX;
            protected int lastY;
            protected int lastHeight;

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
                int nameColor = row.custom ? 0xFFFFFF : 0xF0F0F0;
                screen.renderFirstColumn(graphics, row, nameColor, this);
                drawCell(graphics, 1, row.motion.isBlank()
                        ? Component.translatable("pmx.settings.value.unset").getString()
                        : row.motion, 0xF0F0F0);
                drawCheckbox(graphics, 2, row.motionLoop);
                drawCell(graphics, 3, row.camera.isBlank()
                        ? Component.translatable("pmx.settings.value.unset").getString()
                        : row.camera, 0xF0F0F0);
                drawCheckbox(graphics, 4, row.cameraLock);
                drawCell(graphics, 5, row.music.isBlank()
                        ? Component.translatable("pmx.settings.value.unset").getString()
                        : row.music, 0xF0F0F0);
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
                        case 2 -> {
                            if (screen.allowMotionLoopToggle(row)) {
                                row.motionLoop = !row.motionLoop;
                            }
                        }
                        case 4 -> row.cameraLock = !row.cameraLock;
                        case 1 -> screen.openMotionPicker(row);
                        case 3 -> screen.openCameraPicker(row);
                        case 5 -> screen.openMusicPicker(row);
                        default -> {
                        }
                    }
                    if (i == 2 || i == 4) {
                        screen.saveSettings();
                    }
                    return true;
                }
                return false;
            }

            protected void drawCell(GuiGraphics graphics, int col, String text, int color) {
                int left = colX[col];
                int width = colW[col];
                int textWidth = screen.font.width(text);
                int drawX = left + Math.max(2, (width - textWidth) / 2);
                graphics.drawString(screen.font, text, drawX, lastY + 6, color, false);
            }

            protected void drawCheckbox(GuiGraphics graphics, int col, boolean checked) {
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
        }
    }
}
