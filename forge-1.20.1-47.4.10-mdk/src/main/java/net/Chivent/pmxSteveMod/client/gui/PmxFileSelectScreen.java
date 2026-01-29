package net.Chivent.pmxSteveMod.client.gui;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.function.Consumer;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.AbstractSelectionList;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.gui.narration.NarrationElementOutput;
import net.minecraft.network.chat.Component;
import org.jetbrains.annotations.NotNull;

public class PmxFileSelectScreen extends Screen {
    private final Screen parent;
    private final Path folder;
    private final String[] extensions;
    private final Consumer<Path> onSelect;
    private final Component titleText;
    private final Component openFolderLabel;
    private PmxFileList list;
    private PmxFileWatcher watcher;
    private volatile boolean rescanRequested;
    private Path pendingSelection;
    private boolean explicitNoneSelected;

    public PmxFileSelectScreen(Screen parent, Path folder, String[] extensions,
                               Component titleText, Component openFolderLabel, Consumer<Path> onSelect) {
        super(titleText);
        this.parent = parent;
        this.folder = folder;
        this.extensions = extensions;
        this.onSelect = onSelect;
        this.titleText = titleText;
        this.openFolderLabel = openFolderLabel;
    }

    @Override
    protected void init() {
        int listTop = 40;
        int listBottom = this.height - 36;
        list = new PmxFileList(this.minecraft, this.width - 40, listBottom - listTop,
                listTop, listBottom, 20);
        list.setLeftPos(20);
        list.setRenderBackground(false);
        list.setRenderTopAndBottom(false);
        addWidget(list);
        pendingSelection = null;
        explicitNoneSelected = false;
        reloadList();
        startWatcher();
        int rowY = this.height - 28;
        int btnWidth = GuiUtil.FOOTER_BUTTON_WIDTH;
        int btnGap = 10;
        int totalWidth = btnWidth * 2 + btnGap;
        int leftX = (this.width - totalWidth) / 2;
        addRenderableWidget(net.minecraft.client.gui.components.Button.builder(
                openFolderLabel, b -> {
                    if (folder != null) {
                        net.minecraft.Util.getPlatform().openFile(folder.toFile());
                    }
                }).bounds(leftX, rowY, btnWidth, 20).build());
        addRenderableWidget(net.minecraft.client.gui.components.Button.builder(
                Component.translatable("pmx.button.done"), b -> confirmSelection())
                .bounds(leftX + btnWidth + btnGap, rowY, btnWidth, 20).build());
    }

    @Override
    public void tick() {
        if (rescanRequested) {
            rescanRequested = false;
            reloadList();
        }
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        this.renderBackground(graphics);
        if (list != null) {
            list.render(graphics, mouseX, mouseY, partialTick);
        }
        graphics.drawCenteredString(this.font, titleText, this.width / 2, 14, 0xFFFFFF);
        super.render(graphics, mouseX, mouseY, partialTick);
    }

    @Override
    public void renderBackground(@NotNull GuiGraphics graphics) {
        super.renderBackground(graphics);
    }

    @Override
    public void onClose() {
        stopWatcher();
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

    @Override
    public void removed() {
        stopWatcher();
        super.removed();
    }

    private void reloadList() {
        if (list == null) return;
        list.replaceEntries(listFiles());
        if (pendingSelection != null) {
            list.setSelected(pendingSelection);
        }
    }

    private void startWatcher() {
        stopWatcher();
        if (folder == null) return;
        watcher = new PmxFileWatcher(
                folder,
                1,
                false,
                p -> p != null && hasExtension(p.getFileName().toString()),
                () -> rescanRequested = true
        );
        watcher.start();
    }

    private void stopWatcher() {
        if (watcher != null) {
            watcher.stop();
            watcher = null;
        }
    }

    private List<Path> listFiles() {
        List<Path> out = new ArrayList<>();
        if (folder == null || !Files.isDirectory(folder)) {
            return out;
        }
        try (var stream = Files.list(folder)) {
            stream.filter(p -> hasExtension(p.getFileName().toString()))
                    .sorted(Comparator.comparing(p -> p.getFileName().toString().toLowerCase(Locale.ROOT)))
                    .forEach(out::add);
        } catch (Exception ignored) {
        }
        return out;
    }

    private boolean hasExtension(String name) {
        String lower = name.toLowerCase(Locale.ROOT);
        for (String ext : extensions) {
            if (lower.endsWith(ext)) return true;
        }
        return false;
    }

    private void select(Path path) {
        pendingSelection = path;
        explicitNoneSelected = false;
        if (list != null) {
            list.setSelected(path);
        }
    }

    private void selectNone() {
        pendingSelection = null;
        explicitNoneSelected = true;
        if (list != null) {
            list.setSelectedNone();
        }
    }

    private void confirmSelection() {
        onSelect.accept(pendingSelection);
        onClose();
    }

    public boolean isExplicitNoneSelected() {
        return explicitNoneSelected;
    }

    private final class PmxFileList extends AbstractSelectionList<PmxFileList.Entry> {
        private Path selectedPath;
        private boolean selectedNone;

        public PmxFileList(Minecraft minecraft, int width, int height,
                           int y0, int y1, int itemHeight) {
            super(minecraft, width, height, y0, y1, itemHeight);
        }

        public void addNoneEntry() {
            addEntry(new PmxNoneEntry());
        }

        public void addFileEntry(Path path) {
            addEntry(new PmxFileEntry(path));
        }

        public void replaceEntries(List<Path> files) {
            clearEntries();
            addNoneEntry();
            for (Path p : files) {
                addFileEntry(p);
            }
            if (selectedPath != null) {
                setSelected(selectedPath);
            } else if (selectedNone) {
                setSelectedNone();
            }
        }

        public void setSelected(Path path) {
            selectedPath = path;
            selectedNone = false;
            for (Entry e : children()) {
                if (e instanceof PmxFileEntry fe && path != null && path.equals(fe.path)) {
                    setSelected(e);
                    return;
                }
            }
        }

        public void setSelectedNone() {
            selectedPath = null;
            selectedNone = true;
            for (Entry e : children()) {
                if (e instanceof PmxNoneEntry) {
                    setSelected(e);
                    return;
                }
            }
        }

        @Override
        public void updateNarration(@NotNull NarrationElementOutput output) {
            // No narration for now.
        }

        private abstract static class Entry extends AbstractSelectionList.Entry<Entry> {}

        private final class PmxFileEntry extends Entry {
            private final Path path;

            private PmxFileEntry(Path path) {
                this.path = path;
            }

            @Override
            public void render(@NotNull GuiGraphics graphics, int index, int y, int x, int width, int height,
                               int mouseX, int mouseY, boolean hovered, float partialTick) {
                if (this == PmxFileList.this.getSelected()) {
                    graphics.fill(x, y, x + width, y + height, 0x33000000);
                }
                String name = path.getFileName().toString();
                graphics.drawString(font, name, x + 6, y + 6, 0xE0E0E0, false);
            }

            @Override
            public boolean mouseClicked(double mouseX, double mouseY, int button) {
                select(path);
                return true;
            }
        }

        private final class PmxNoneEntry extends Entry {
            @Override
            public void render(@NotNull GuiGraphics graphics, int index, int y, int x, int width, int height,
                               int mouseX, int mouseY, boolean hovered, float partialTick) {
                if (this == PmxFileList.this.getSelected()) {
                    graphics.fill(x, y, x + width, y + height, 0x33000000);
                }
                graphics.drawString(font, Component.translatable("pmx.settings.value.unset"), x + 6, y + 6, 0xAAAAAA, false);
            }

            @Override
            public boolean mouseClicked(double mouseX, double mouseY, int button) {
                selectNone();
                return true;
            }
        }
    }
}
