package net.Chivent.pmxSteveMod.client.gui;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import net.minecraft.world.entity.Pose;
import org.jetbrains.annotations.NotNull;

import java.util.Locale;

public class PmxAddStateScreen extends Screen {
    private final PmxStateSettingsScreen parent;
    private int presetLabelY;

    public PmxAddStateScreen(PmxStateSettingsScreen parent) {
        super(Component.translatable("pmx.screen.add_state.title"));
        this.parent = parent;
    }

    @Override
    protected void init() {
        int centerX = this.width / 2;
        Pose[] allPoses = Pose.values();
        int poseCount = 0;
        for (Pose pose : allPoses) {
            if (pose != Pose.STANDING) {
                poseCount++;
            }
        }
        Pose[] poses = new Pose[poseCount];
        int poseIndex = 0;
        for (Pose pose : allPoses) {
            if (pose != Pose.STANDING) {
                poses[poseIndex++] = pose;
            }
        }
        int columns = 2;
        int rows = (poses.length + columns - 1) / columns;
        int rowHeight = 22;
        int presetSpacing = 6;
        int presetLabelHeight = 12;
        int footerHeight = 20;
        int footerGap = 10;
        int presetHeight = rows * rowHeight;
        int totalHeight = presetLabelHeight + presetSpacing + presetHeight + footerGap + footerHeight;

        presetLabelY = (this.height - totalHeight) / 2;
        int presetStartY = presetLabelY + presetLabelHeight + presetSpacing;
        int footerY = presetStartY + presetHeight + footerGap;

        int buttonWidth = 96;
        int buttonHeight = 20;
        int buttonGap = 8;
        int gridWidth = columns * buttonWidth + (columns - 1) * buttonGap;
        int startX = centerX - (gridWidth / 2);
        for (int i = 0; i < poses.length; i++) {
            int col = i % columns;
            int row = i / columns;
            int x = startX + col * (buttonWidth + buttonGap);
            int y = presetStartY + row * rowHeight;
            String label = formatPoseName(poses[i]);
            addRenderableWidget(Button.builder(Component.literal(label), b -> {
                parent.addCustomState(label);
                Minecraft.getInstance().setScreen(parent);
            })
                    .bounds(x, y, buttonWidth, buttonHeight).build());
        }

        addRenderableWidget(Button.builder(Component.translatable("pmx.button.cancel"), b -> onClose())
                .bounds(centerX - 48, footerY, 96, 20).build());
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        this.renderBackground(graphics);
        graphics.drawCenteredString(this.font, Component.translatable("pmx.settings.label.state_presets"),
                this.width / 2, presetLabelY, 0xC0C0C0);
        super.render(graphics, mouseX, mouseY, partialTick);
    }

    @Override
    public void onClose() {
        Minecraft.getInstance().setScreen(parent);
    }

    private static String formatPoseName(Pose pose) {
        String raw = pose.name().toLowerCase(Locale.ROOT);
        String[] parts = raw.split("_");
        StringBuilder out = new StringBuilder();
        for (String part : parts) {
            if (part.isEmpty()) continue;
            if (!out.isEmpty()) out.append(' ');
            out.append(Character.toUpperCase(part.charAt(0)));
            if (part.length() > 1) out.append(part.substring(1));
        }
        return out.toString();
    }
}
