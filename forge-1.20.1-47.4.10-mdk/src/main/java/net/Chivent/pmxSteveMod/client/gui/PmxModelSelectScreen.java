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
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public class PmxModelSelectScreen extends Screen {
    private final Screen parent;
    private PmxModelList list;
    private Path modelDir;

    public PmxModelSelectScreen(Screen parent) {
        super(Component.literal("Select PMX Model"));
        this.parent = parent;
    }

    @Override
    protected void init() {
        modelDir = PmxViewer.get().getUserModelDir();
        list = new PmxModelList(this, this.minecraft, this.width, this.height, 50, this.height - 40, 20);
        list.setRenderBackground(false);
        addWidget(list);
        reloadList();

        addRenderableWidget(Button.builder(Component.literal("Open PMX Folder"), b -> {
            if (modelDir != null) {
                Util.getPlatform().openFile(modelDir.toFile());
            }
        }).bounds(this.width / 2 - 155, this.height - 32, 150, 20).build());
        addRenderableWidget(Button.builder(toggleLabel(), b -> {
            PmxViewer viewer = PmxViewer.get();
            viewer.setPmxVisible(!viewer.isPmxVisible());
            b.setMessage(toggleLabel());
        }).bounds(this.width / 2 + 5, this.height - 32, 150, 20).build());
        addRenderableWidget(Button.builder(Component.literal("Rescan"), b -> reloadList())
                .bounds(this.width / 2 - 155, this.height - 56, 150, 20)
                .build());
        addRenderableWidget(Button.builder(Component.literal("Back"), b -> {
            if (this.minecraft != null) {
                this.minecraft.setScreen(parent);
            }
        }).bounds(this.width / 2 + 5, this.height - 56, 150, 20).build());
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
                    Component.literal("PMX folder: " + modelDir),
                    10, this.height - 80, 0xA0A0A0, false);
        }
        super.render(graphics, mouseX, mouseY, partialTick);
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

    private Component toggleLabel() {
        String state = PmxViewer.get().isPmxVisible() ? "Show PMX: ON" : "Show PMX: OFF";
        return Component.literal(state);
    }

    private static final class PmxModelList extends AbstractSelectionList<PmxModelList.PmxModelEntry> {
        private final PmxModelSelectScreen screen;

        public PmxModelList(PmxModelSelectScreen screen, net.minecraft.client.Minecraft minecraft,
                            int width, int height, int y0, int y1, int itemHeight) {
            super(minecraft, width, height, y0, y1, itemHeight);
            this.screen = screen;
        }

        public void replaceEntries(List<Path> models) {
            clearEntries();
            for (Path p : models) {
                addEntry(new PmxModelEntry(screen, p));
            }
        }

        @Override
        protected int getScrollbarPosition() {
            return this.width - 6;
        }

        @Override
        public void updateNarration(@NotNull NarrationElementOutput output) {
        }

        private static final class PmxModelEntry extends AbstractSelectionList.Entry<PmxModelEntry> {
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
                viewer.reloadSelectedModel();
                if (screen.minecraft != null) {
                    screen.minecraft.setScreen(screen.parent);
                }
                return true;
            }
        }
    }
}
