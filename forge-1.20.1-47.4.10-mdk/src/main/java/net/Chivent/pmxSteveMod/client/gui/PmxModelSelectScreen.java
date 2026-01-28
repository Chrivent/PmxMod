package net.Chivent.pmxSteveMod.client.gui;

import com.mojang.blaze3d.platform.InputConstants;
import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraft.Util;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.AbstractSelectionList;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.gui.narration.NarrationElementOutput;
import net.minecraft.network.chat.Component;
import net.minecraft.util.FormattedCharSequence;
import net.minecraft.util.Mth;
import org.jetbrains.annotations.NotNull;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public class PmxModelSelectScreen extends Screen {
    private static final int INFO_PANEL_WIDTH = 190;
    private static final int INFO_PANEL_PADDING = 12;
    private static final int INFO_PANEL_INSET = 4;
    private final Screen parent;
    private PmxModelList list;
    private Path modelDir;
    private PmxFileWatcher watcher;
    private volatile boolean rescanRequested = false;
    private double infoScroll = 0.0;
    private int infoScrollMax = 0;

    public PmxModelSelectScreen(Screen parent) {
        super(Component.translatable("pmx.screen.select_model.title"));
        this.parent = parent;
    }

    @Override
    protected void init() {
        modelDir = PmxViewer.get().getUserModelDir();
        int listLeft = INFO_PANEL_WIDTH + INFO_PANEL_PADDING;
        int listWidth = Math.max(100, this.width - listLeft - 10);
        list = new PmxModelList(this, this.minecraft, listWidth, this.height, 50, this.height - 40, 20);
        list.setLeftPos(listLeft);
        list.setRenderBackground(false);
        list.setRenderTopAndBottom(false);
        addWidget(list);
        reloadList();
        startWatcher();

        int rowY = this.height - 32;
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.open_folder"), b -> {
            if (modelDir != null) {
                Util.getPlatform().openFile(modelDir.toFile());
            }
        }).bounds(this.width / 2 - 155, rowY, 150, 20).build());
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.done"), b -> onClose())
                .bounds(this.width / 2 + 5, rowY, 150, 20).build());
    }

    private void reloadList() {
        List<Path> models = listPmxFiles(modelDir);
        list.replaceEntries(models);
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        this.renderBackground(graphics);
        renderInfoPanel(graphics);
        list.render(graphics, mouseX, mouseY, partialTick);
        graphics.drawCenteredString(this.font, this.title, this.width / 2, 15, 0xFFFFFF);
        super.render(graphics, mouseX, mouseY, partialTick);
    }

    @Override
    public boolean mouseScrolled(double mouseX, double mouseY, double delta) {
        if (isMouseInInfoPanel(mouseX, mouseY) && infoScrollMax > 0) {
            double step = this.font.lineHeight * 3.0;
            infoScroll = Mth.clamp(infoScroll - delta * step, 0.0, infoScrollMax);
            return true;
        }
        return super.mouseScrolled(mouseX, mouseY, delta);
    }

    @Override
    public void renderBackground(@NotNull GuiGraphics graphics) {
        GuiUtil.renderDefaultBackground(graphics, this.width, this.height);
    }

    private void renderInfoPanel(@NotNull GuiGraphics graphics) {
        int left = infoPanelLeft();
        int top = infoPanelTop();
        int right = infoPanelRight();
        int bottom = infoPanelBottom();
        graphics.fill(left, top, right, bottom, 0x33000000);

        int contentLeft = left + INFO_PANEL_INSET;
        int contentTop = top + INFO_PANEL_INSET;
        int contentRight = right - INFO_PANEL_INSET;
        int contentBottom = bottom - INFO_PANEL_INSET;
        int contentWidth = contentRight - contentLeft;
        int contentHeight = contentBottom - contentTop;

        List<InfoLine> lines = buildInfoLines(contentWidth);
        int totalHeight = lines.isEmpty() ? 0 : lines.get(lines.size() - 1).y + this.font.lineHeight;
        infoScrollMax = Math.max(0, totalHeight - contentHeight);
        infoScroll = Mth.clamp(infoScroll, 0.0, infoScrollMax);

        graphics.enableScissor(contentLeft, contentTop, contentRight, contentBottom);
        int yOffset = contentTop - (int) infoScroll;
        for (InfoLine line : lines) {
            int y = yOffset + line.y;
            graphics.drawString(this.font, line.text, contentLeft, y, line.color, false);
        }
        graphics.disableScissor();
    }

    private Component valueComponent(String value) {
        if (value == null || value.isBlank()) {
            return Component.translatable("pmx.info.empty");
        }
        return Component.literal(value);
    }

    private List<InfoLine> buildInfoLines(int width) {
        List<InfoLine> lines = new ArrayList<>();
        int y = 0;
        y = addLine(lines, y, Component.translatable("pmx.info.header"), 0xFFFFFF);
        y += 4;

        Path selected = PmxViewer.get().getSelectedModelPath();
        if (selected != null) {
            y = addLabelValueLines(lines, y,
                    Component.translatable("pmx.info.file"),
                    valueComponent(selected.getFileName().toString()),
                    width);
        }

        PmxViewer viewer = PmxViewer.get();
        if (!viewer.isPmxVisible()) {
            addMessageLines(lines, y, Component.translatable("pmx.info.disabled"), width);
            return lines;
        }

        PmxInstance.ModelInfo info = viewer.instance().getModelInfo();
        if (info == null) {
            addMessageLines(lines, y, Component.translatable("pmx.info.none"), width);
            return lines;
        }

        y = addLabelValueLines(lines, y,
                Component.translatable("pmx.info.name"),
                valueComponent(info.name()),
                width);
        y = addLabelValueLines(lines, y,
                Component.translatable("pmx.info.name_en"),
                valueComponent(info.nameEn()),
                width);
        y = addLabelValueLines(lines, y,
                Component.translatable("pmx.info.comment"),
                valueComponent(info.comment()),
                width);
        addLabelValueLines(lines, y,
                Component.translatable("pmx.info.comment_en"),
                valueComponent(info.commentEn()),
                width);
        return lines;
    }

    private int addLine(List<InfoLine> out, int y, Component text, int color) {
        out.add(new InfoLine(text.getVisualOrderText(), color, y));
        return y + this.font.lineHeight;
    }

    private int addLine(List<InfoLine> out, int y, FormattedCharSequence text, int color) {
        out.add(new InfoLine(text, color, y));
        return y + this.font.lineHeight;
    }

    private int addLabelValueLines(List<InfoLine> out, int y, Component label, Component value, int width) {
        y = addLine(out, y, label, 0xB0B0B0);
        y += 2;
        for (var line : this.font.split(value, width)) {
            y = addLine(out, y, line, 0xE0E0E0);
        }
        return y + 4;
    }

    private void addMessageLines(List<InfoLine> out, int y, Component message, int width) {
        for (var line : this.font.split(message, width)) {
            y = addLine(out, y, line, 0xC0C0C0);
        }
    }

    public void resetInfoScroll() {
        infoScroll = 0.0;
    }

    private int infoPanelLeft() {
        return 10;
    }

    private int infoPanelRight() {
        return INFO_PANEL_WIDTH;
    }

    private int infoPanelTop() {
        return 50;
    }

    private int infoPanelBottom() {
        return this.height - 40;
    }

    private boolean isMouseInInfoPanel(double mouseX, double mouseY) {
        return mouseX >= infoPanelLeft() && mouseX <= infoPanelRight()
                && mouseY >= infoPanelTop() && mouseY <= infoPanelBottom();
    }

    private record InfoLine(FormattedCharSequence text, int color, int y) {}

    private void openModelSettings(Path modelPath) {
        if (this.minecraft != null) {
            this.minecraft.setScreen(new PmxSlotSettingsScreen(this, modelPath));
        }
    }

    private void openStateSettings(Path modelPath) {
        if (this.minecraft != null) {
            this.minecraft.setScreen(new PmxStateSettingsScreen(this, modelPath));
        }
    }

    @Override
    public void tick() {
        if (rescanRequested) {
            rescanRequested = false;
            reloadList();
        }
        super.tick();
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

    @Override
    public void removed() {
        stopWatcher();
        super.removed();
    }

    private static List<Path> listPmxFiles(Path dir) {
        List<Path> results = new ArrayList<>();
        if (dir == null) return results;
        try (var stream = Files.walk(dir, 2)) {
            stream.filter(p -> p.toString().toLowerCase().endsWith(".pmx"))
                    .sorted(Comparator.comparing(p -> p.getFileName().toString().toLowerCase()))
                    .forEach(results::add);
        } catch (Exception ignored) {
            return results;
        }
        return results;
    }

    private void startWatcher() {
        stopWatcher();
        if (modelDir == null) return;
        watcher = new PmxFileWatcher(
                modelDir,
                2,
                true,
                p -> p != null && p.toString().toLowerCase().endsWith(".pmx"),
                () -> rescanRequested = true
        );
        watcher.start();
    }

    private void applyIdleMotion() {
        PmxViewer.get().playIdleOrDefault();
    }

    private void stopWatcher() {
        if (watcher != null) {
            watcher.stop();
            watcher = null;
        }
    }


    private static final class PmxModelList extends AbstractSelectionList<PmxModelList.PmxEntry> {
        private final PmxModelSelectScreen screen;

        public PmxModelList(PmxModelSelectScreen screen, net.minecraft.client.Minecraft minecraft,
                            int width, int height, int y0, int y1, int itemHeight) {
            super(minecraft, width, height, y0, y1, itemHeight);
            this.screen = screen;
        }

        public void replaceEntries(List<Path> models) {
            clearEntries();
            addEntry(new PmxNoneEntry(screen));
            for (Path p : models) {
                addEntry(new PmxModelEntry(screen, p));
            }
            updateSelectedFromViewer();
        }

        private void updateSelectedFromViewer() {
            PmxViewer viewer = PmxViewer.get();
            if (viewer.isPmxVisible()) {
                Path selected = viewer.getSelectedModelPath();
                if (selected == null) return;
                for (int i = 0; i < this.children().size(); i++) {
                    if (this.children().get(i) instanceof PmxModelEntry entry) {
                        if (selected.equals(entry.modelPath)) {
                            setSelected(entry);
                            break;
                        }
                    }
                }
            } else {
                for (int i = 0; i < this.children().size(); i++) {
                    if (this.children().get(i) instanceof PmxNoneEntry) {
                        setSelected(this.children().get(i));
                        break;
                    }
                }
            }
        }

        @Override
        protected int getScrollbarPosition() {
            return this.getRight() - 6;
        }

        @Override
        public int getRowWidth() {
            return Math.max(100, this.width - 12);
        }

        @Override
        public void updateNarration(@NotNull NarrationElementOutput output) {
        }

        private abstract static class PmxEntry extends AbstractSelectionList.Entry<PmxEntry> {
        }

        private static final class PmxModelEntry extends PmxEntry {
            private final PmxModelSelectScreen screen;
            private final Path modelPath;
            private static final int BUTTON_WIDTH = 58;
            private static final int BUTTON_GAP = 4;
            private static final int BUTTON_PADDING = 6;
            private int lastX;
            private int lastY;
            private int lastWidth;
            private int lastHeight;

            private PmxModelEntry(PmxModelSelectScreen screen, Path modelPath) {
                this.screen = screen;
                this.modelPath = modelPath;
            }

            @Override
            public void render(@NotNull GuiGraphics graphics, int index, int y, int x, int width, int height,
                               int mouseX, int mouseY, boolean hovered, float partialTick) {
                lastX = x;
                lastY = y;
                lastWidth = width;
                lastHeight = height;
                String name = modelPath.getFileName().toString();
                graphics.drawString(screen.font, name, x + 6, y + 2, 0xFFFFFF, false);
                int btnRight = x + width - BUTTON_PADDING;
                int totalWidth = BUTTON_WIDTH * 2 + BUTTON_GAP;
                int leftA = btnRight - totalWidth;
                int leftB = leftA + BUTTON_WIDTH + BUTTON_GAP;
                int btnTop = y + 1;
                int btnBottom = y + height - 1;

                Component labelA = Component.translatable("pmx.button.slot_settings");
                Component labelB = Component.translatable("pmx.button.state_settings");
                drawSmallButton(graphics, leftA, btnTop, btnBottom - btnTop,
                        mouseX, mouseY, labelA);
                drawSmallButton(graphics, leftB, btnTop, btnBottom - btnTop,
                        mouseX, mouseY, labelB);
            }

            @Override
            public boolean mouseClicked(double mouseX, double mouseY, int button) {
                int btnRight = lastX + lastWidth - BUTTON_PADDING;
                int totalWidth = BUTTON_WIDTH * 2 + BUTTON_GAP;
                int leftA = btnRight - totalWidth;
                int leftB = leftA + BUTTON_WIDTH + BUTTON_GAP;
                int rowTop = lastY;
                int rowBottom = lastY + lastHeight;
                if (mouseX >= leftA && mouseX <= leftA + BUTTON_WIDTH && mouseY >= rowTop && mouseY <= rowBottom) {
                    screen.openModelSettings(modelPath);
                    return true;
                }
                if (mouseX >= leftB && mouseX <= leftB + BUTTON_WIDTH && mouseY >= rowTop && mouseY <= rowBottom) {
                    screen.openStateSettings(modelPath);
                    return true;
                }
                PmxViewer viewer = PmxViewer.get();
                viewer.setSelectedModelPath(modelPath);
                viewer.setPmxVisible(true);
                viewer.reloadSelectedModel();
                screen.applyIdleMotion();
                if (screen.list != null) {
                    screen.list.setSelected(this);
                }
                screen.resetInfoScroll();
                return true;
            }

            private void drawSmallButton(GuiGraphics graphics, int left, int top, int height,
                                         int mouseX, int mouseY, Component label) {
                int right = left + PmxModelEntry.BUTTON_WIDTH;
                int bottom = top + height;
                boolean hover = mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom;
                int border = hover ? 0xFFCCCCCC : 0xFF777777;
                int fill = hover ? 0x40222222 : 0x20222222;
                graphics.fill(left, top, right, bottom, fill);
                graphics.renderOutline(left, top, PmxModelEntry.BUTTON_WIDTH, height, border);
                int labelX = left + (PmxModelEntry.BUTTON_WIDTH - screen.font.width(label)) / 2;
                int labelY = top + 2;
                graphics.drawString(screen.font, label, labelX, labelY, 0xFFFFFF, false);
            }
        }

        private static final class PmxNoneEntry extends PmxEntry {
            private final PmxModelSelectScreen screen;

            private PmxNoneEntry(PmxModelSelectScreen screen) {
                this.screen = screen;
            }

            @Override
            public void render(@NotNull GuiGraphics graphics, int index, int y, int x, int width, int height,
                               int mouseX, int mouseY, boolean hovered, float partialTick) {
                graphics.drawString(screen.font,
                        Component.translatable("pmx.screen.select_model.disable"),
                        x + 6, y + 2, 0xAAAAAA, false);
            }

            @Override
            public boolean mouseClicked(double mouseX, double mouseY, int button) {
                PmxViewer viewer = PmxViewer.get();
                viewer.setPmxVisible(false);
                if (screen.list != null) {
                    screen.list.setSelected(this);
                }
                screen.resetInfoScroll();
                return true;
            }
        }
    }
}
