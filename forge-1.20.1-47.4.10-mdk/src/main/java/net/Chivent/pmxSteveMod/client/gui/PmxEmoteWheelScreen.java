package net.Chivent.pmxSteveMod.client.gui;

import net.Chivent.pmxSteveMod.client.emote.PmxEmoteWheelState;
import net.Chivent.pmxSteveMod.client.input.PmxKeyMappings;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import org.lwjgl.glfw.GLFW;

public class PmxEmoteWheelScreen extends Screen {
    private static final int SLOT_COUNT = 6;
    private static final int DEADZONE_RADIUS = 38;
    private static final int WHEEL_RADIUS = 110;

    private final Screen parent;
    private int selectedSlot = -1;
    private boolean cancelled;

    public PmxEmoteWheelScreen(Screen parent) {
        super(Component.translatable("pmx.screen.emote_wheel.title"));
        this.parent = parent;
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
    public void render(GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        updateSelection(mouseX, mouseY);

        int centerX = this.width / 2;
        int centerY = this.height / 2;
        drawCircle(graphics, centerX, centerY, WHEEL_RADIUS, 0x99000000);
        drawWheelBoundaries(graphics, centerX, centerY, 0x80FFFFFF);
        for (int i = 0; i < SLOT_COUNT; i++) {
            double angle = Math.toRadians(-90.0 + (360.0 / SLOT_COUNT) * i);
            int slotX = centerX + (int) Math.round(Math.cos(angle) * (WHEEL_RADIUS * 0.72));
            int slotY = centerY + (int) Math.round(Math.sin(angle) * (WHEEL_RADIUS * 0.72));
            int color = (i == selectedSlot) ? 0xCCFFFFFF : 0xAAFFFFFF;
            graphics.drawCenteredString(this.font, Integer.toString(i + 1), slotX, slotY - 4, color);
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
        double dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < DEADZONE_RADIUS || dist > WHEEL_RADIUS) {
            selectedSlot = -1;
            return;
        }
        double angle = Math.toDegrees(Math.atan2(dy, dx));
        double step = 360.0 / SLOT_COUNT;
        double normalized = (angle + 90.0 + step / 2.0 + 360.0) % 360.0;
        int slot = (int) (normalized / step);
        if (slot < 0 || slot >= SLOT_COUNT) {
            selectedSlot = -1;
        } else {
            selectedSlot = slot;
        }
    }

    private void closeWithSelection() {
        if (!cancelled && selectedSlot >= 0) {
            PmxEmoteWheelState.setLastSelectedSlot(selectedSlot);
        }
        onClose();
    }

    private void cancel() {
        cancelled = true;
        onClose();
    }

    private boolean isWheelKeyDown() {
        long window = Minecraft.getInstance().getWindow().getWindow();
        int key = PmxKeyMappings.EMOTE_WHEEL.getKey().getValue();
        return org.lwjgl.glfw.GLFW.glfwGetKey(window, key) == GLFW.GLFW_PRESS;
    }

    private void drawWheelBoundaries(GuiGraphics graphics, int cx, int cy, int color) {
        double step = 360.0 / SLOT_COUNT;
        for (int i = 0; i < SLOT_COUNT; i++) {
            double angle = Math.toRadians(-90.0 + (step * i) - (step / 2.0));
            double cos = Math.cos(angle);
            double sin = Math.sin(angle);
            int x1 = cx + (int) Math.round(cos * DEADZONE_RADIUS);
            int y1 = cy + (int) Math.round(sin * DEADZONE_RADIUS);
            int x2 = cx + (int) Math.round(cos * WHEEL_RADIUS);
            int y2 = cy + (int) Math.round(sin * WHEEL_RADIUS);
            drawLine(graphics, x1, y1, x2, y2, color);
        }
    }

    private void drawCircle(GuiGraphics graphics, int cx, int cy, int radius, int color) {
        int r2 = radius * radius;
        for (int dy = -radius; dy <= radius; dy++) {
            int dx = (int) Math.floor(Math.sqrt(r2 - (dy * dy)));
            graphics.fill(cx - dx, cy + dy, cx + dx + 1, cy + dy + 1, color);
        }
    }

    private void drawLine(GuiGraphics graphics, int x1, int y1, int x2, int y2, int color) {
        int dx = Math.abs(x2 - x1);
        int dy = Math.abs(y2 - y1);
        int steps = Math.max(dx, dy);
        if (steps == 0) {
            graphics.fill(x1, y1, x1 + 1, y1 + 1, color);
            return;
        }
        double xStep = (x2 - x1) / (double) steps;
        double yStep = (y2 - y1) / (double) steps;
        double x = x1;
        double y = y1;
        for (int i = 0; i <= steps; i++) {
            graphics.fill((int) Math.round(x), (int) Math.round(y),
                    (int) Math.round(x) + 1, (int) Math.round(y) + 1, color);
            x += xStep;
            y += yStep;
        }
    }
}
