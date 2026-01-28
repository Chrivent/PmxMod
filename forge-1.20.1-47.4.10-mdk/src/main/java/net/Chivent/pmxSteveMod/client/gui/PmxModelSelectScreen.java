package net.Chivent.pmxSteveMod.client.gui;

import com.mojang.blaze3d.platform.InputConstants;
import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraft.Util;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.AbstractSelectionList;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.gui.narration.NarrationElementOutput;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.ResourceLocation;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import net.minecraft.util.FormattedCharSequence;
import net.minecraft.util.Mth;
import net.minecraft.world.inventory.InventoryMenu;
import org.jetbrains.annotations.NotNull;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.WatchEvent;
import java.nio.file.WatchKey;
import java.nio.file.WatchService;
import java.nio.file.StandardWatchEventKinds;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

public class PmxModelSelectScreen extends Screen {
    private static final ResourceLocation BACKGROUND_SPRITE_ID =
            ResourceLocation.fromNamespaceAndPath("minecraft", "block/obsidian");
    private static final int INFO_PANEL_WIDTH = 190;
    private static final int INFO_PANEL_PADDING = 12;
    private static final int INFO_PANEL_INSET = 4;
    private final Screen parent;
    private PmxModelList list;
    private Path modelDir;
    private WatchService watchService;
    private Thread watchThread;
    private volatile boolean rescanRequested = false;
    private final Set<Path> watchedDirs = new HashSet<>();
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
        }).bounds(this.width / 2 - 100, rowY, 200, 20).build());
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
        renderTiledBackground(graphics);
        int top = 32;
        int bottom = this.height - 32;
        if (bottom > top) {
            graphics.fillGradient(0, top, this.width, bottom, 0x55000000, 0x77000000);
        }
    }

    private void renderTiledBackground(@NotNull GuiGraphics graphics) {
        Minecraft mc = Minecraft.getInstance();
        TextureAtlasSprite sprite = mc.getTextureAtlas(InventoryMenu.BLOCK_ATLAS).apply(BACKGROUND_SPRITE_ID);
        int tile = 16;
        int top = 32;
        int bottom = this.height - 32;
        for (int y = 0; y < top; y += tile) {
            int h = Math.min(tile, top - y);
            for (int x = 0; x < this.width; x += tile) {
                int w = Math.min(tile, this.width - x);
                graphics.blit(x, y, 0, w, h, sprite);
            }
        }
        for (int y = bottom; y < this.height; y += tile) {
            int h = Math.min(tile, this.height - y);
            for (int x = 0; x < this.width; x += tile) {
                int w = Math.min(tile, this.width - x);
                graphics.blit(x, y, 0, w, h, sprite);
            }
        }
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
        this.minecraft.setScreen(new PmxModelSettingsScreen(this, modelPath));
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
        try {
            watchService = modelDir.getFileSystem().newWatchService();
            registerDir(modelDir);
            try (var stream = Files.list(modelDir)) {
                stream.filter(Files::isDirectory).forEach(this::registerDir);
            } catch (Exception ignored) {
            }
        } catch (Exception ignored) {
            return;
        }
        watchThread = new Thread(this::watchLoop, "pmx-model-watch");
        watchThread.setDaemon(true);
        watchThread.start();
    }

    private void watchLoop() {
        while (watchService != null) {
            try {
                WatchKey key = watchService.poll(250, TimeUnit.MILLISECONDS);
                if (key == null) continue;
                Path dir = (Path) key.watchable();
                for (WatchEvent<?> event : key.pollEvents()) {
                    WatchEvent.Kind<?> kind = event.kind();
                    if (kind == StandardWatchEventKinds.OVERFLOW) continue;
                    @SuppressWarnings("unchecked")
                    WatchEvent<Path> ev = (WatchEvent<Path>) event;
                    Path child = dir.resolve(ev.context());
                    if (Files.isDirectory(child) && kind == StandardWatchEventKinds.ENTRY_CREATE) {
                        registerDir(child);
                    }
                    if (child.toString().toLowerCase().endsWith(".pmx")) {
                        rescanRequested = true;
                    }
                }
                key.reset();
            } catch (InterruptedException ignored) {
                return;
            } catch (Exception ignored) {
                rescanRequested = true;
            }
        }
    }

    private void registerDir(Path dir) {
        if (dir == null || watchedDirs.contains(dir)) return;
        try {
            dir.register(watchService,
                    StandardWatchEventKinds.ENTRY_CREATE,
                    StandardWatchEventKinds.ENTRY_DELETE,
                    StandardWatchEventKinds.ENTRY_MODIFY);
            watchedDirs.add(dir);
        } catch (Exception ignored) {
        }
    }

    private void stopWatcher() {
        if (watchThread != null) {
            watchThread.interrupt();
            watchThread = null;
        }
        if (watchService != null) {
            try { watchService.close(); } catch (Exception ignored) {}
            watchService = null;
        }
        watchedDirs.clear();
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
            private static final int BUTTON_WIDTH = 54;
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
                int btnLeft = btnRight - BUTTON_WIDTH;
                int btnTop = y + 1;
                int btnBottom = y + height - 1;
                boolean btnHover = mouseX >= btnLeft && mouseX <= btnRight && mouseY >= btnTop && mouseY <= btnBottom;
                int border = btnHover ? 0xFFCCCCCC : 0xFF777777;
                int fill = btnHover ? 0x40222222 : 0x20222222;
                graphics.fill(btnLeft, btnTop, btnRight, btnBottom, fill);
                graphics.renderOutline(btnLeft, btnTop, btnRight - btnLeft, btnBottom - btnTop, border);
                Component label = Component.translatable("pmx.button.settings");
                int labelX = btnLeft + (BUTTON_WIDTH - screen.font.width(label)) / 2;
                int labelY = y + 2;
                graphics.drawString(screen.font, label, labelX, labelY, 0xFFFFFF, false);
            }

            @Override
            public boolean mouseClicked(double mouseX, double mouseY, int button) {
                int btnRight = lastX + lastWidth - BUTTON_PADDING;
                int btnLeft = btnRight - BUTTON_WIDTH;
                int rowTop = lastY;
                int rowBottom = lastY + lastHeight;
                if (mouseX >= btnLeft && mouseX <= btnRight && mouseY >= rowTop && mouseY <= rowBottom) {
                    screen.openModelSettings(modelPath);
                    return true;
                }
                PmxViewer viewer = PmxViewer.get();
                viewer.setSelectedModelPath(modelPath);
                viewer.setPmxVisible(true);
                viewer.reloadSelectedModel();
                if (screen.list != null) {
                    screen.list.setSelected(this);
                }
                screen.resetInfoScroll();
                return true;
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
