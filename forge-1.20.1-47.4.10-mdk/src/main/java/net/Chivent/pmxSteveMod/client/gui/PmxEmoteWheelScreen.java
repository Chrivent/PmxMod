package net.Chivent.pmxSteveMod.client.gui;

import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import net.minecraft.Util;
import org.jetbrains.annotations.NotNull;
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
    private final boolean[] slotLoop = new boolean[SLOT_COUNT];

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
        int deadZone = getDeadZoneRadius(wheelRadius);
        GuiUtil.drawWheelRing(graphics, centerX, centerY, wheelRadius, deadZone, 0x99000000);
        if (selectedSlot >= 0 && slotActive[selectedSlot]) {
            GuiUtil.drawWheelSelection(graphics, centerX, centerY, wheelRadius, deadZone, selectedSlot, SLOT_COUNT, 0x33FFFFFF);
        }
        float thickness = Math.max(1.0f, wheelRadius * 0.01f);
        GuiUtil.drawWheelBoundaries(graphics, centerX, centerY, wheelRadius, deadZone, SLOT_COUNT, thickness, 0x80FFFFFF);
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

        long now = Util.getMillis();
        for (int i = 0; i < SLOT_COUNT; i++) {
            if (isSlotPlaying(i)) {
                drawCometSpinner(graphics, centerX, centerY, wheelRadius, deadZone, i, now);
            }
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
        java.nio.file.Path current = viewer.instance().getCurrentMotionPath();
        if (current != null) {
            java.nio.file.Path normalized = motionPath;
            try {
                normalized = motionPath.toAbsolutePath().normalize();
            } catch (Exception ignored) {
            }
            if (normalized.equals(current)) {
                viewer.playIdleOrDefault();
                return;
            }
        }
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
        viewer.instance().playMotion(motionPath, musicPath, cameraPath, slotLoop[slot], true);
        viewer.setCurrentSlotMotion(slot);
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
            slotLoop[i] = false;
        }
        for (var row : rows) {
            if (row.slotIndex < 0 || row.slotIndex >= SLOT_COUNT) continue;
            if (row.motion != null && !row.motion.isBlank()) {
                slotLabels[row.slotIndex] = row.motion;
                slotActive[row.slotIndex] = true;
                slotMotionFiles[row.slotIndex] = row.motion;
                slotMusicFiles[row.slotIndex] = row.music == null ? "" : row.music;
                slotCameraFiles[row.slotIndex] = row.camera == null ? "" : row.camera;
                slotLoop[row.slotIndex] = row.motionLoop;
            }
        }
    }

    private boolean isWheelKeyDown() {
        long window = Minecraft.getInstance().getWindow().getWindow();
        int key = PmxKeyMappings.EMOTE_WHEEL.getKey().getValue();
        return org.lwjgl.glfw.GLFW.glfwGetKey(window, key) == GLFW.GLFW_PRESS;
    }

    private boolean isSlotPlaying(int slot) {
        net.Chivent.pmxSteveMod.viewer.PmxViewer viewer = net.Chivent.pmxSteveMod.viewer.PmxViewer.get();
        if (!viewer.isPmxVisible()) return false;
        return viewer.getCurrentSlotIndex() == slot;
    }

    private void drawCometSpinner(GuiGraphics graphics, int centerX, int centerY,
                                  int wheelRadius, int deadZone, int slot, long nowMillis) {
        double sliceAngle = 360.0 / SLOT_COUNT;
        double midDeg = -90.0 + sliceAngle * slot;
        double midRad = Math.toRadians(midDeg);

        int ringRadius = deadZone + Math.round((wheelRadius - deadZone) * 0.48f);
        int orbitRadius = Math.max(12, Math.round((wheelRadius - deadZone) * 0.22f));
        float size = Math.max(2.0f, wheelRadius * 0.014f);
        float periodMs = 900.0f;
        float t = (nowMillis % (long) periodMs) / periodMs;
        double spin = t * Math.PI * 2.0;

        int baseX = centerX + (int) Math.round(Math.cos(midRad) * ringRadius);
        int baseY = centerY + (int) Math.round(Math.sin(midRad) * ringRadius);

        int x = baseX + (int) Math.round(Math.cos(spin) * orbitRadius);
        int y = baseY + (int) Math.round(Math.sin(spin) * orbitRadius);

        float tailThickness = Math.max(1.0f, size);
        int tailOuter = Math.round(orbitRadius + tailThickness);
        int tailInner = Math.max(0, Math.round(orbitRadius - tailThickness));
        double headDeg = Math.toDegrees(spin);
        double tailLen = 180.0;
        int tailSegments = 6;
        float tailEndAlpha = 0.0f;
        for (int i = 0; i < tailSegments; i++) {
            double segT0 = i / (double) tailSegments;
            double segT1 = (i + 1) / (double) tailSegments;
            double segStart = headDeg - tailLen * segT1;
            double segEnd = headDeg - tailLen * segT0;
            float alpha = 1.0f - (float) segT0;
            if (i == tailSegments - 1) {
                tailEndAlpha = alpha;
            }
            int a = Math.max(0, Math.min(255, Math.round(alpha * 255.0f)));
            int color = (a << 24) | 0xFFFFFF;
            GuiUtil.drawRingSector(graphics, baseX, baseY, tailOuter, tailInner,
                    segStart, segEnd, Math.max(8, (int) Math.round((segEnd - segStart) * 0.6)), color);
        }

        double tailEndRad = Math.toRadians(headDeg - tailLen);
        int tailX = baseX + (int) Math.round(Math.cos(tailEndRad) * orbitRadius);
        int tailY = baseY + (int) Math.round(Math.sin(tailEndRad) * orbitRadius);
        int tailEndA = Math.max(0, Math.min(255, Math.round(tailEndAlpha * 255.0f)));
        GuiUtil.drawSmoothCircle(graphics, tailX, tailY, size, (tailEndA << 24) | 0xFFFFFF);
        GuiUtil.drawSmoothCircle(graphics, x, y, size, 0xFFFFFFFF);
    }

    private int getWheelRadius() {
        int base = Math.min(this.width, this.height);
        int scaled = (int) Math.round(base * 0.36);
        return Math.max(MIN_WHEEL_RADIUS, scaled);
    }

    private int getDeadZoneRadius(int wheelRadius) {
        return GuiUtil.getWheelDeadZoneRadius(wheelRadius);
    }

    private int getLabelRadius(int wheelRadius) {
        return (int) Math.round(wheelRadius * 0.72);
    }
}
