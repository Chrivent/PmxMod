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
    private static final int DEADZONE_RADIUS = 24;
    private static final int SLOT_RADIUS = 78;
    private static final int SLOT_HALF_SIZE = 18;

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
        graphics.fill(0, 0, this.width, this.height, 0x66000000);

        int centerX = this.width / 2;
        int centerY = this.height / 2;
        for (int i = 0; i < SLOT_COUNT; i++) {
            double angle = Math.toRadians(-90.0 + (360.0 / SLOT_COUNT) * i);
            int slotX = centerX + (int) Math.round(Math.cos(angle) * SLOT_RADIUS);
            int slotY = centerY + (int) Math.round(Math.sin(angle) * SLOT_RADIUS);

            int color = (i == selectedSlot) ? 0xCCFFFFFF : 0xAA202020;
            graphics.fill(slotX - SLOT_HALF_SIZE, slotY - SLOT_HALF_SIZE,
                    slotX + SLOT_HALF_SIZE, slotY + SLOT_HALF_SIZE, color);
            graphics.drawCenteredString(this.font, Integer.toString(i + 1), slotX, slotY - 4, 0xFFFFFFFF);
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
        if (dist < DEADZONE_RADIUS) {
            selectedSlot = -1;
            return;
        }
        double angle = Math.toDegrees(Math.atan2(dy, dx));
        double normalized = (angle + 450.0) % 360.0;
        int slot = (int) (normalized / (360.0 / SLOT_COUNT));
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
}
