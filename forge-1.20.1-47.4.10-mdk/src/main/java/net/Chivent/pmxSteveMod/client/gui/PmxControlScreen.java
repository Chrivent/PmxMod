package net.Chivent.pmxSteveMod.client.gui;

import com.mojang.blaze3d.platform.InputConstants;
import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.CycleButton;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import org.jetbrains.annotations.NotNull;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public class PmxControlScreen extends Screen {
    private final Screen parent;
    private Path modelDir;

    public PmxControlScreen(Screen parent) {
        super(Component.literal("PMX Control"));
        this.parent = parent;
    }

    @Override
    protected void init() {
        int centerX = this.width / 2;
        int y = this.height / 4;

        PmxViewer viewer = PmxViewer.get();
        modelDir = viewer.getUserModelDir();
        List<Path> models = listPmxFiles(modelDir);

        if (!models.isEmpty()) {
            Path initial = models.contains(viewer.getSelectedModelPath())
                    ? viewer.getSelectedModelPath()
                    : models.get(0);
            viewer.setSelectedModelPath(initial);
            addRenderableWidget(CycleButton.<Path>builder(
                    p -> Component.literal(p.getFileName().toString()))
                    .withValues(models)
                    .withInitialValue(initial)
                    .create(centerX - 100, y, 200, 20, Component.literal("Model"),
                            (btn, value) -> {
                                viewer.setSelectedModelPath(value);
                                viewer.reloadSelectedModel();
                            }));
        } else {
            Button empty = Button.builder(Component.literal("No PMX files found"), b -> {})
                    .bounds(centerX - 100, y, 200, 20)
                    .build();
            empty.active = false;
            addRenderableWidget(empty);
        }

        Button toggleButton = Button.builder(toggleLabel(), b -> {
            viewer.setPmxVisible(!viewer.isPmxVisible());
            b.setMessage(toggleLabel());
        }).bounds(centerX - 100, y + 30, 200, 20).build();
        addRenderableWidget(toggleButton);
    }

    private Component toggleLabel() {
        String state = PmxViewer.get().isPmxVisible() ? "ON" : "OFF";
        return Component.literal("Show PMX: " + state);
    }

    @Override
    public boolean isPauseScreen() {
        return false;
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
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        super.render(graphics, mouseX, mouseY, partialTick);
        if (modelDir != null) {
            graphics.drawString(this.font,
                    Component.literal("PMX folder: " + modelDir),
                    10, 10, 0xFFFFFF, false);
        }
        graphics.drawString(this.font,
                Component.literal("Press key or Esc to close"),
                10, 24, 0xA0A0A0, false);
    }

    private static List<Path> listPmxFiles(Path dir) {
        List<Path> results = new ArrayList<>();
        if (dir == null) return results;
        try (var stream = Files.list(dir)) {
            stream.filter(p -> p.toString().toLowerCase().endsWith(".pmx"))
                    .sorted(Comparator.comparing(p -> p.getFileName().toString().toLowerCase()))
                    .forEach(results::add);
        } catch (Exception ignored) {
            return results;
        }
        return results;
    }
}
