package net.Chivent.pmxSteveMod.client.gui;

import com.mojang.blaze3d.platform.InputConstants;
import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraft.Util;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.AbstractSelectionList;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.gui.narration.NarrationElementOutput;
import net.minecraft.network.chat.Component;
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
    private final Screen parent;
    private PmxModelList list;
    private Path modelDir;
    private WatchService watchService;
    private Thread watchThread;
    private volatile boolean rescanRequested = false;
    private final Set<Path> watchedDirs = new HashSet<>();

    public PmxModelSelectScreen(Screen parent) {
        super(Component.translatable("pmx.screen.select_model.title"));
        this.parent = parent;
    }

    @Override
    protected void init() {
        modelDir = PmxViewer.get().getUserModelDir();
        list = new PmxModelList(this, this.minecraft, this.width, this.height, 50, this.height - 40, 20);
        list.setRenderBackground(false);
        addWidget(list);
        reloadList();
        startWatcher();

        int rowY = this.height - 32;
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.open_folder"), b -> {
            if (modelDir != null) {
                Util.getPlatform().openFile(modelDir.toFile());
            }
        }).bounds(this.width / 2 - 155, rowY, 150, 20).build());
    }

    private void reloadList() {
        List<Path> models = listPmxFiles(modelDir);
        list.replaceEntries(models);
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        this.renderBackground(graphics);
        list.render(graphics, mouseX, mouseY, partialTick);
        graphics.drawCenteredString(this.font, this.title, this.width / 2, 15, 0xFFFFFF);
        if (modelDir != null) {
            graphics.drawString(this.font,
                    Component.translatable("pmx.screen.select_model.folder", modelDir),
                    10, this.height - 56, 0xA0A0A0, false);
        }
        super.render(graphics, mouseX, mouseY, partialTick);
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
            return this.width - 6;
        }

        @Override
        public void updateNarration(@NotNull NarrationElementOutput output) {
        }

        private abstract static class PmxEntry extends AbstractSelectionList.Entry<PmxEntry> {
        }

        private static final class PmxModelEntry extends PmxEntry {
            private final PmxModelSelectScreen screen;
            private final Path modelPath;

            private PmxModelEntry(PmxModelSelectScreen screen, Path modelPath) {
                this.screen = screen;
                this.modelPath = modelPath;
            }

            @Override
            public void render(@NotNull GuiGraphics graphics, int index, int y, int x, int width, int height,
                               int mouseX, int mouseY, boolean hovered, float partialTick) {
                String name = modelPath.getFileName().toString();
                graphics.drawString(screen.font, name, x + 6, y + 2, 0xFFFFFF, false);
            }

            @Override
            public boolean mouseClicked(double mouseX, double mouseY, int button) {
                PmxViewer viewer = PmxViewer.get();
                viewer.setSelectedModelPath(modelPath);
                viewer.setPmxVisible(true);
                viewer.reloadSelectedModel();
                if (screen.list != null) {
                    screen.list.setSelected(this);
                }
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
                return true;
            }
        }
    }
}
