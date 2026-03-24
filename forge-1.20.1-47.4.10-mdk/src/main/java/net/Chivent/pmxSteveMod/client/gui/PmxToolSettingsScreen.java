package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

import net.Chivent.pmxSteveMod.client.settings.PmxToolSettingsStore;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.components.EditBox;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import org.jetbrains.annotations.NotNull;

public class PmxToolSettingsScreen extends Screen {
    private static final int GRID_COLS = 3;
    private static final int GRID_ROWS = 3;
    private static final int AXIS_COUNT = 3;
    private static final int ROW_COUNT_MAX = 4;

    private static final class Category {
        final String headerKey;
        final boolean hasAlt;
        Category(String headerKey, boolean hasAlt) {
            this.headerKey = headerKey;
            this.hasAlt = hasAlt;
        }
    }

    private static final Category[] CATEGORIES = new Category[] {
            new Category("pmx.tool.header.handheld", false),
            new Category("pmx.tool.header.rod", false),
            new Category("pmx.tool.header.bow", false),
            new Category("pmx.tool.header.crossbow", false),
            new Category("pmx.tool.header.shield", true),
            new Category("pmx.tool.header.trident", true),
            new Category("pmx.tool.header.spyglass", false),
            new Category("pmx.tool.header.goat_horn", false),
            new Category("pmx.tool.header.brush", false)
    };

    private static final String[] ROW_LABEL_KEYS = new String[] {
            "pmx.tool.row.pos",
            "pmx.tool.row.rot",
            "pmx.tool.row.delta_pos",
            "pmx.tool.row.delta_rot"
    };

    private final Screen parent;
    @SuppressWarnings("unused")
    private final Path modelPath;
    private final EditBox[][][] fields = new EditBox[CATEGORIES.length][ROW_COUNT_MAX][AXIS_COUNT];
    private final List<EditBox> allFields = new ArrayList<>();
    private final int[][][] baseFieldX = new int[CATEGORIES.length][ROW_COUNT_MAX][AXIS_COUNT];
    private final int[][][] baseFieldY = new int[CATEGORIES.length][ROW_COUNT_MAX][AXIS_COUNT];
    private int contentScroll = 0;
    private int contentScrollMax = 0;

    public PmxToolSettingsScreen(Screen parent, Path modelPath) {
        super(Component.translatable("pmx.screen.tool_settings.title"));
        this.parent = parent;
        this.modelPath = modelPath;
    }

    @Override
    protected void init() {
        this.clearWidgets();
        allFields.clear();
        layoutFields();
        loadValues();
        int rowY = this.height - 32;
        int itemWidth = Math.min(GuiUtil.FOOTER_BUTTON_WIDTH, Math.max(80, this.width / 4));
        int rightX = this.width - 10 - itemWidth;
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.done"), b -> onClose())
                .bounds(rightX, rowY, itemWidth, 20).build());
        updateScrollMetrics();
        updateFieldLayout();
    }

    private void layoutFields() {
        int panelTop = 40;
        int cellPadding = 8;
        int rowHeight = 20;
        int rowGap = 6;
        int labelWidth = 34;
        int headerHeight = this.font.lineHeight + 2;
        int rowStartOffset = headerHeight + 4;
        int maxRows = ROW_COUNT_MAX;
        int cellHeight = rowStartOffset + (maxRows * rowHeight) + ((maxRows - 1) * rowGap) + 4;

        int panelLeft = 10;
        int panelRight = this.width - 10;
        int cellWidth = (panelRight - panelLeft) / GRID_COLS;

        for (int i = 0; i < CATEGORIES.length; i++) {
            int row = i / GRID_COLS;
            int col = i % GRID_COLS;
            int cellLeft = panelLeft + cellWidth * col;
            int cellTop = panelTop + cellHeight * row;
            int labelX = cellLeft + cellPadding;
            int boxX = labelX + labelWidth;
            int boxAreaWidth = cellWidth - cellPadding * 2 - labelWidth;
            int boxGap = 4;
            int boxWidth = (boxAreaWidth - boxGap * (AXIS_COUNT - 1)) / AXIS_COUNT;
            for (int r = 0; r < ROW_COUNT_MAX; r++) {
                if (r >= 2 && !CATEGORIES[i].hasAlt) {
                    continue;
                }
                int y = cellTop + rowStartOffset + r * (rowHeight + rowGap);
                for (int a = 0; a < AXIS_COUNT; a++) {
                    int x = boxX + a * (boxWidth + boxGap);
                    EditBox box = new EditBox(this.font, x, y, boxWidth, rowHeight, Component.empty());
                    box.setMaxLength(16);
                    box.setValue("0");
                    box.setFilter(PmxToolSettingsScreen::isNumericInput);
                    box.setResponder(ignored -> updateLiveValues());
                    fields[i][r][a] = box;
                    baseFieldX[i][r][a] = x;
                    baseFieldY[i][r][a] = y;
                    allFields.add(box);
                    addRenderableWidget(box);
                }
            }
        }
    }

    private static boolean isNumericInput(String text) {
        if (text == null || text.isEmpty()) return true;
        return text.matches("-?\\d*(\\.\\d*)?");
    }

    private void loadValues() {
        PmxToolSettingsStore.ToolOffsets offsets = PmxToolSettingsStore.get().get(modelPath);
        applyCategoryOffsets(0, offsets.handheld);
        applyCategoryOffsets(1, offsets.rod);
        applyCategoryOffsets(2, offsets.bow);
        applyCategoryOffsets(3, offsets.crossbow);
        applyCategoryOffsets(4, offsets.shield);
        applyCategoryOffsets(5, offsets.trident);
        applyCategoryOffsets(6, offsets.spyglass);
        applyCategoryOffsets(7, offsets.goatHorn);
        applyCategoryOffsets(8, offsets.brush);
        updateLiveValues();
    }

    private void applyCategoryOffsets(int category, PmxToolSettingsStore.ToolCategoryOffsets offsets) {
        if (offsets == null) return;
        applyOffsetsToFields(category, offsets.base, 0);
        applyOffsetsToFields(category, offsets.alt, 2);
    }

    private void applyOffsetsToFields(int category, PmxToolSettingsStore.ToolOffset offset, int rowBase) {
        if (offset == null) return;
        setField(category, rowBase, 0, offset.posX);
        setField(category, rowBase, 1, offset.posY);
        setField(category, rowBase, 2, offset.posZ);
        setField(category, rowBase + 1, 0, offset.rotX);
        setField(category, rowBase + 1, 1, offset.rotY);
        setField(category, rowBase + 1, 2, offset.rotZ);
    }

    private void setField(int category, int row, int axis, float value) {
        if (row < 0 || row >= ROW_COUNT_MAX) return;
        EditBox box = fields[category][row][axis];
        if (box != null) {
            box.setValue(trimFloat(value));
        }
    }

    private static String trimFloat(float v) {
        if (v == (long) v) {
            return Long.toString((long) v);
        }
        String text = Float.toString(v);
        if (text.endsWith(".0")) {
            return text.substring(0, text.length() - 2);
        }
        return text;
    }

    private PmxToolSettingsStore.ToolCategoryOffsets readCategoryOffsets(int category) {
        PmxToolSettingsStore.ToolCategoryOffsets out = new PmxToolSettingsStore.ToolCategoryOffsets();
        out.base = readOffset(category, 0);
        out.alt = readOffset(category, 2);
        return out;
    }

    private PmxToolSettingsStore.ToolOffset readOffset(int category, int rowBase) {
        PmxToolSettingsStore.ToolOffset out = new PmxToolSettingsStore.ToolOffset();
        out.posX = readField(category, rowBase, 0);
        out.posY = readField(category, rowBase, 1);
        out.posZ = readField(category, rowBase, 2);
        out.rotX = readField(category, rowBase + 1, 0);
        out.rotY = readField(category, rowBase + 1, 1);
        out.rotZ = readField(category, rowBase + 1, 2);
        return out;
    }

    private float readField(int category, int row, int axis) {
        if (row < 0 || row >= ROW_COUNT_MAX) return 0.0f;
        EditBox box = fields[category][row][axis];
        if (box == null) return 0.0f;
        String text = box.getValue();
        if (text == null || text.isBlank() || "-".equals(text) || ".".equals(text) || "-.".equals(text)) {
            return 0.0f;
        }
        try {
            return Float.parseFloat(text);
        } catch (NumberFormatException ignored) {
            return 0.0f;
        }
    }

    private void updateLiveValues() {
        PmxToolSettingsStore.ToolOffsets out = new PmxToolSettingsStore.ToolOffsets();
        out.handheld = readCategoryOffsets(0);
        out.rod = readCategoryOffsets(1);
        out.bow = readCategoryOffsets(2);
        out.crossbow = readCategoryOffsets(3);
        out.shield = readCategoryOffsets(4);
        out.trident = readCategoryOffsets(5);
        out.spyglass = readCategoryOffsets(6);
        out.goatHorn = readCategoryOffsets(7);
        out.brush = readCategoryOffsets(8);
        PmxToolSettingsStore.get().setLive(modelPath, out);
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        this.renderBackground(graphics);
        graphics.drawCenteredString(this.font, this.title, this.width / 2, 15, 0xFFFFFF);
        renderTopPanel(graphics);
        updateFieldLayout();
        super.render(graphics, mouseX, mouseY, partialTick);
    }

    private void renderTopPanel(@NotNull GuiGraphics graphics) {
        int panelTop = 40;
        int cellPadding = 8;
        int rowHeight = 20;
        int rowGap = 6;
        int headerHeight = this.font.lineHeight + 2;
        int rowStartOffset = headerHeight + 4;
        int maxRows = ROW_COUNT_MAX;
        int cellHeight = rowStartOffset + (maxRows * rowHeight) + ((maxRows - 1) * rowGap) + 4;
        int panelLeft = 10;
        int panelRight = this.width - 10;
        int cellWidth = (panelRight - panelLeft) / GRID_COLS;
        int panelBottom = panelTop + cellHeight * GRID_ROWS;
        int viewportTop = contentTop();
        int viewportBottom = contentBottom();
        int yOffset = -contentScroll;

        graphics.enableScissor(panelLeft, viewportTop, panelRight, viewportBottom);

        int lineColor = 0x66FFFFFF;
        for (int c = 1; c < GRID_COLS; c++) {
            int x = panelLeft + cellWidth * c;
            graphics.fill(x, panelTop + yOffset, x + 1, panelBottom + yOffset, lineColor);
        }
        for (int r = 1; r < GRID_ROWS; r++) {
            int y = panelTop + cellHeight * r + yOffset;
            graphics.fill(panelLeft, y, panelRight, y + 1, lineColor);
        }

        for (int i = 0; i < CATEGORIES.length; i++) {
            int row = i / GRID_COLS;
            int col = i % GRID_COLS;
            int cellLeft = panelLeft + cellWidth * col;
            int cellTop = panelTop + cellHeight * row + yOffset;
            int headerX = cellLeft + cellPadding;
            graphics.drawString(this.font, Component.translatable(CATEGORIES[i].headerKey), headerX, cellTop, 0xFFFFFF, false);

            int labelX = cellLeft + cellPadding;
            int labelY = cellTop + rowStartOffset + 6;
            for (int r = 0; r < ROW_COUNT_MAX; r++) {
                if (r >= 2 && !CATEGORIES[i].hasAlt) continue;
                int y = labelY + r * (rowHeight + rowGap);
                graphics.drawString(this.font, Component.translatable(ROW_LABEL_KEYS[r]), labelX, y, 0xB0B0B0, false);
            }
        }
        graphics.disableScissor();
    }

    private int contentTop() {
        return 40;
    }

    private int contentBottom() {
        return this.height - 40;
    }

    private int contentHeight() {
        int panelTop = 40;
        int rowHeight = 20;
        int rowGap = 6;
        int headerHeight = this.font.lineHeight + 2;
        int rowStartOffset = headerHeight + 4;
        int maxRows = ROW_COUNT_MAX;
        int cellHeight = rowStartOffset + (maxRows * rowHeight) + ((maxRows - 1) * rowGap) + 4;
        int panelBottom = panelTop + cellHeight * GRID_ROWS;
        return panelBottom - panelTop;
    }

    private void updateScrollMetrics() {
        int viewportHeight = Math.max(0, contentBottom() - contentTop());
        contentScrollMax = Math.max(0, contentHeight() - viewportHeight);
        if (contentScroll < 0) contentScroll = 0;
        if (contentScroll > contentScrollMax) contentScroll = contentScrollMax;
    }

    private void updateFieldLayout() {
        int top = contentTop();
        int bottom = contentBottom();
        for (int i = 0; i < CATEGORIES.length; i++) {
            for (int r = 0; r < ROW_COUNT_MAX; r++) {
                for (int a = 0; a < AXIS_COUNT; a++) {
                    EditBox box = fields[i][r][a];
                    if (box == null) continue;
                    int x = baseFieldX[i][r][a];
                    int y = baseFieldY[i][r][a] - contentScroll;
                    boolean visible = y >= top && y + box.getHeight() <= bottom;
                    box.setX(x);
                    if (visible) {
                        box.setY(y);
                    } else {
                        box.setY(-1000);
                        if (box.isFocused()) {
                            box.setFocused(false);
                        }
                    }
                    box.active = visible;
                }
            }
        }
    }

    @Override
    public boolean mouseScrolled(double mouseX, double mouseY, double delta) {
        if (mouseY >= contentTop() && mouseY <= contentBottom() && contentScrollMax > 0) {
            int step = Math.max(12, this.font.lineHeight * 3);
            contentScroll = (int) Math.max(0, Math.min(contentScrollMax, contentScroll - delta * step));
            updateFieldLayout();
            return true;
        }
        return super.mouseScrolled(mouseX, mouseY, delta);
    }

    @Override
    public void renderBackground(@NotNull GuiGraphics graphics) {
        GuiUtil.renderDefaultBackground(graphics, this.width, this.height);
    }

    @Override
    public void onClose() {
        PmxToolSettingsStore.ToolOffsets out = new PmxToolSettingsStore.ToolOffsets();
        out.handheld = readCategoryOffsets(0);
        out.rod = readCategoryOffsets(1);
        out.bow = readCategoryOffsets(2);
        out.crossbow = readCategoryOffsets(3);
        out.shield = readCategoryOffsets(4);
        out.trident = readCategoryOffsets(5);
        out.spyglass = readCategoryOffsets(6);
        out.goatHorn = readCategoryOffsets(7);
        out.brush = readCategoryOffsets(8);
        PmxToolSettingsStore.get().save(modelPath, out);
        PmxToolSettingsStore.get().clearLive(modelPath);
        if (this.minecraft != null) {
            this.minecraft.setScreen(parent);
        }
    }
}
