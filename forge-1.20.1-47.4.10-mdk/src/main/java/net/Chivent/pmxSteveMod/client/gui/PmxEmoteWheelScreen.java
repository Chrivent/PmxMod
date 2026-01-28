package net.Chivent.pmxSteveMod.client.gui;

import net.Chivent.pmxSteveMod.client.emote.PmxEmoteWheelState;
import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.BufferBuilder;
import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import com.mojang.blaze3d.vertex.Tesselator;
import com.mojang.blaze3d.vertex.VertexFormat;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.network.chat.Component;
import org.jetbrains.annotations.NotNull;
import org.joml.Matrix4f;
import org.lwjgl.glfw.GLFW;

public class PmxEmoteWheelScreen extends Screen {
    private static final int SLOT_COUNT = 6;
    private static final int MIN_WHEEL_RADIUS = 90;

    private final Screen parent;
    private int selectedSlot = -1;
    private boolean cancelled;
    private final String[] slotLabels = new String[SLOT_COUNT];
    private final boolean[] slotActive = new boolean[SLOT_COUNT];
    private final String[] slotMotionFiles = new String[SLOT_COUNT];
    private final String[] slotMusicFiles = new String[SLOT_COUNT];
    private final String[] slotCameraFiles = new String[SLOT_COUNT];

    public PmxEmoteWheelScreen(Screen parent) {
        super(Component.translatable("pmx.screen.emote_wheel.title"));
        this.parent = parent;
        loadSlotLabels();
    }

    @Override
    public boolean isPauseScreen() {
        return false;
    }

    @Override
    public void tick() {
        if (!isWheelKeyDown()) {
            closeWithSelection();
        }
    }

    @Override
    public boolean keyPressed(int keyCode, int scanCode, int modifiers) {
        if (keyCode == GLFW.GLFW_KEY_ESCAPE) {
            cancel();
            return true;
        }
        return super.keyPressed(keyCode, scanCode, modifiers);
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        updateSelection(mouseX, mouseY);

        int centerX = this.width / 2;
        int centerY = this.height / 2;
        int wheelRadius = getWheelRadius();
        int labelRadius = getLabelRadius(wheelRadius);
        drawWheelRing(graphics, centerX, centerY, wheelRadius, 0x99000000);
        if (selectedSlot >= 0 && slotActive[selectedSlot]) {
            drawSelectedSector(graphics, centerX, centerY, wheelRadius, selectedSlot, 0x22FFFFFF);
        }
        drawWheelBoundaries(graphics, centerX, centerY, 0x80FFFFFF);
        for (int i = 0; i < SLOT_COUNT; i++) {
            double angle = Math.toRadians(-90.0 + (360.0 / SLOT_COUNT) * i);
            int slotX = centerX + (int) Math.round(Math.cos(angle) * labelRadius);
            int slotY = centerY + (int) Math.round(Math.sin(angle) * labelRadius);
            String label = slotLabels[i] != null ? slotLabels[i]
                    : Component.translatable("pmx.emote.empty").getString();
            int color;
            if (!slotActive[i]) {
                color = 0x66FFFFFF;
            } else {
                color = (i == selectedSlot) ? 0xCCFFFFFF : 0xAAFFFFFF;
            }
            graphics.drawCenteredString(this.font, label, slotX, slotY - 4, color);
        }

        super.render(graphics, mouseX, mouseY, partialTick);
    }

    @Override
    public void onClose() {
        Minecraft.getInstance().setScreen(parent);
    }

    private void updateSelection(double mouseX, double mouseY) {
        double dx = mouseX - this.width / 2.0;
        double dy = mouseY - this.height / 2.0;
        int wheelRadius = getWheelRadius();
        int deadZone = getDeadZoneRadius(wheelRadius);
        double dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < deadZone || dist > wheelRadius) {
            selectedSlot = -1;
            return;
        }
        double angle = Math.toDegrees(Math.atan2(dy, dx));
        double step = 360.0 / SLOT_COUNT;
        double normalized = (angle + 90.0 + step / 2.0 + 360.0) % 360.0;
        int slot = (int) (normalized / step);
        if (slot < 0 || slot >= SLOT_COUNT || !slotActive[slot]) {
            selectedSlot = -1;
        } else {
            selectedSlot = slot;
        }
    }

    private void closeWithSelection() {
        if (!cancelled && selectedSlot >= 0 && slotActive[selectedSlot]) {
            PmxEmoteWheelState.setLastSelectedSlot(selectedSlot);
            executeSlot(selectedSlot);
        }
        onClose();
    }

    private void cancel() {
        cancelled = true;
        onClose();
    }

    private void executeSlot(int slot) {
        if (slot < 0 || slot >= SLOT_COUNT) return;
        String motionFile = slotMotionFiles[slot];
        if (motionFile == null || motionFile.isBlank()) return;
        net.Chivent.pmxSteveMod.viewer.PmxViewer viewer = net.Chivent.pmxSteveMod.viewer.PmxViewer.get();
        java.nio.file.Path dir = viewer.getUserMotionDir();
        java.nio.file.Path motionPath = dir.resolve(motionFile);
        String musicFile = slotMusicFiles[slot];
        java.nio.file.Path musicPath = null;
        if (musicFile != null && !musicFile.isBlank()) {
            java.nio.file.Path musicDir = viewer.getUserMusicDir();
            musicPath = musicDir.resolve(musicFile);
        }
        String cameraFile = slotCameraFiles[slot];
        java.nio.file.Path cameraPath = null;
        if (cameraFile != null && !cameraFile.isBlank()) {
            java.nio.file.Path cameraDir = viewer.getUserCameraDir();
            cameraPath = cameraDir.resolve(cameraFile);
        }
        viewer.setPmxVisible(true);
        viewer.instance().playMotion(motionPath, musicPath, cameraPath);
    }

    private void loadSlotLabels() {
        java.nio.file.Path modelPath = net.Chivent.pmxSteveMod.viewer.PmxViewer.get().getSelectedModelPath();
        java.util.List<net.Chivent.pmxSteveMod.client.settings.PmxModelSettingsStore.RowData> rows =
                net.Chivent.pmxSteveMod.client.settings.PmxModelSettingsStore.get().load(modelPath);
        for (int i = 0; i < SLOT_COUNT; i++) {
            slotLabels[i] = Component.translatable("pmx.emote.empty").getString();
            slotActive[i] = false;
            slotMotionFiles[i] = "";
            slotMusicFiles[i] = "";
            slotCameraFiles[i] = "";
        }
        for (var row : rows) {
            if (row.slotIndex < 0 || row.slotIndex >= SLOT_COUNT) continue;
            if (row.motion != null && !row.motion.isBlank()) {
                slotLabels[row.slotIndex] = row.motion;
                slotActive[row.slotIndex] = true;
                slotMotionFiles[row.slotIndex] = row.motion;
                slotMusicFiles[row.slotIndex] = row.music == null ? "" : row.music;
                slotCameraFiles[row.slotIndex] = row.camera == null ? "" : row.camera;
            }
        }
    }

    private boolean isWheelKeyDown() {
        long window = Minecraft.getInstance().getWindow().getWindow();
        int key = PmxKeyMappings.EMOTE_WHEEL.getKey().getValue();
        return org.lwjgl.glfw.GLFW.glfwGetKey(window, key) == GLFW.GLFW_PRESS;
    }

    private void drawWheelBoundaries(GuiGraphics graphics, int cx, int cy, int color) {
        int wheelRadius = getWheelRadius();
        int deadZone = getDeadZoneRadius(wheelRadius);
        double step = 360.0 / SLOT_COUNT;
        float thickness = Math.max(1.0f, wheelRadius * 0.01f);
        for (int i = 0; i < SLOT_COUNT; i++) {
            double angle = Math.toRadians(-90.0 + (step * i) - (step / 2.0));
            double cos = Math.cos(angle);
            double sin = Math.sin(angle);
            int x1 = cx + (int) Math.round(cos * deadZone);
            int y1 = cy + (int) Math.round(sin * deadZone);
            int x2 = cx + (int) Math.round(cos * wheelRadius);
            int y2 = cy + (int) Math.round(sin * wheelRadius);
            drawThickLine(graphics, x1, y1, x2, y2, thickness, color);
        }
    }

    private void drawSelectedSector(GuiGraphics graphics, int cx, int cy, int wheelRadius, int slot, int color) {
        int deadZone = getDeadZoneRadius(wheelRadius);
        double step = 360.0 / SLOT_COUNT;
        double centerDeg = -90.0 + (step * slot);
        double startDeg = centerDeg - (step / 2.0);
        double endDeg = centerDeg + (step / 2.0);
        int segments = Math.max(12, (int) Math.round(step * 0.6));

        GuiUtil.drawRingSector(graphics, cx, cy, wheelRadius, deadZone, startDeg, endDeg, segments, color);
    }

    private void drawWheelRing(GuiGraphics graphics, int cx, int cy, int radius, int color) {
        int inner = getDeadZoneRadius(radius);
        int segments = Math.max(48, (int) Math.round(radius * 1.2));

        GuiUtil.drawRingSector(graphics, cx, cy, radius, inner, -90.0, 270.0, segments, color);
    }

    private void drawThickLine(GuiGraphics graphics, float x1, float y1, float x2, float y2, float thickness, int color) {
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = (float) Math.sqrt(dx * dx + dy * dy);
        if (len <= 0.0001f) return;
        float nx = -dy / len;
        float ny = dx / len;
        float half = thickness * 0.5f;
        float ox = nx * half;
        float oy = ny * half;

        GuiUtil.ColorFloats c = GuiUtil.toColorFloats(color);
        Matrix4f pose = graphics.pose().last().pose();
        BufferBuilder builder = beginColorBuilder();
        builder.vertex(pose, x1 - ox, y1 - oy, 0).color(c.r(), c.g(), c.b(), c.a()).endVertex();
        builder.vertex(pose, x1 + ox, y1 + oy, 0).color(c.r(), c.g(), c.b(), c.a()).endVertex();
        builder.vertex(pose, x2 + ox, y2 + oy, 0).color(c.r(), c.g(), c.b(), c.a()).endVertex();
        builder.vertex(pose, x2 - ox, y2 - oy, 0).color(c.r(), c.g(), c.b(), c.a()).endVertex();
        endColorBuilder();
    }

    private BufferBuilder beginColorBuilder() {
        RenderSystem.setShader(GameRenderer::getPositionColorShader);
        RenderSystem.enableBlend();
        BufferBuilder builder = Tesselator.getInstance().getBuilder();
        builder.begin(VertexFormat.Mode.QUADS, DefaultVertexFormat.POSITION_COLOR);
        return builder;
    }

    private void endColorBuilder() {
        Tesselator.getInstance().end();
        RenderSystem.disableBlend();
    }

    private int getWheelRadius() {
        int base = Math.min(this.width, this.height);
        int scaled = (int) Math.round(base * 0.36);
        return Math.max(MIN_WHEEL_RADIUS, scaled);
    }

    private int getDeadZoneRadius(int wheelRadius) {
        return (int) Math.round(wheelRadius * 0.42);
    }

    private int getLabelRadius(int wheelRadius) {
        return (int) Math.round(wheelRadius * 0.72);
    }
}
