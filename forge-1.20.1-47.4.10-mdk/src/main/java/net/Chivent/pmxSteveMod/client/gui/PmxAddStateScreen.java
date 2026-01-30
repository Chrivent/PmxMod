package net.Chivent.pmxSteveMod.client.gui;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.components.AbstractSelectionList;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import net.minecraft.world.entity.Pose;
import org.jetbrains.annotations.NotNull;

import java.util.ArrayList;
import java.util.List;

public class PmxAddStateScreen extends Screen {
    private final PmxStateSettingsScreen parent;
    private PmxStateList list;
    private Component selectedPose;
    private Component selectedAnim;
    private Button doneButton;
    private int listTop;
    private int listBottom;

    public PmxAddStateScreen(PmxStateSettingsScreen parent) {
        super(Component.translatable("pmx.screen.add_state.title"));
        this.parent = parent;
    }

    @Override
    protected void init() {
        List<Category> categories = buildCategories();

        listTop = 40;
        listBottom = this.height - 36;
        list = new PmxStateList(this.minecraft, this.width - 40, listBottom - listTop,
                listTop, listBottom, 22, categories);
        list.setLeftPos(20);
        list.setRenderBackground(false);
        list.setRenderTopAndBottom(false);
        addWidget(list);

        GuiUtil.FooterButtons footer = GuiUtil.footerButtons(this.width, 80, 10);
        int rowY = this.height - 28;
        addRenderableWidget(Button.builder(Component.translatable("pmx.button.cancel"), b -> onClose())
                .bounds(footer.leftX(), rowY, footer.btnWidth(), 20).build());
        doneButton = addRenderableWidget(Button.builder(Component.translatable("pmx.button.done"), b -> confirmSelection())
                .bounds(footer.leftX() + footer.btnWidth() + footer.gap(), rowY, footer.btnWidth(), 20).build());
        doneButton.active = false;
    }

    @Override
    public void render(@NotNull GuiGraphics graphics, int mouseX, int mouseY, float partialTick) {
        this.renderBackground(graphics);
        if (list != null) {
            list.render(graphics, mouseX, mouseY, partialTick);
            int sepX = list.separatorX();
            graphics.fill(sepX, listTop, sepX + 1, listBottom, 0x66FFFFFF);
        }
        super.render(graphics, mouseX, mouseY, partialTick);
    }

    @Override
    public void onClose() {
        Minecraft.getInstance().setScreen(parent);
    }

    private void confirmSelection() {
        if (selectedPose == null || selectedAnim == null) return;
        String label = selectedPose.getString() + " / " + selectedAnim.getString();
        parent.addCustomState(label);
        Minecraft.getInstance().setScreen(parent);
    }

    private void onPickPose(Component label) {
        selectedPose = label;
        updateDoneState();
    }

    private void onPickAnim(Component label) {
        selectedAnim = label;
        updateDoneState();
    }

    private void updateDoneState() {
        if (doneButton != null) {
            doneButton.active = selectedPose != null && selectedAnim != null;
        }
    }

    private List<Category> buildCategories() {
        Pose[] poses = new Pose[] {
                Pose.FALL_FLYING,
                Pose.SLEEPING,
                Pose.SWIMMING,
                Pose.SPIN_ATTACK,
                Pose.CROUCHING,
                Pose.DYING,
                Pose.SITTING
        };
        List<Category> out = new ArrayList<>();
        out.add(new Category(Component.translatable("pmx.settings.label.pose_presets"), poseLabels(poses)));
        out.add(new Category(Component.translatable("pmx.settings.label.anim_movement"), List.of(
                Component.translatable("pmx.anim.walk"),
                Component.translatable("pmx.anim.run"),
                Component.translatable("pmx.anim.jump"),
                Component.translatable("pmx.anim.fall"),
                Component.translatable("pmx.anim.land")
        )));
        out.add(new Category(Component.translatable("pmx.settings.label.anim_combat"), List.of(
                Component.translatable("pmx.anim.attack"),
                Component.translatable("pmx.anim.mine"),
                Component.translatable("pmx.anim.hurt")
        )));
        out.add(new Category(Component.translatable("pmx.settings.label.anim_item"), List.of(
                Component.translatable("pmx.anim.use_item"),
                Component.translatable("pmx.anim.eat_drink"),
                Component.translatable("pmx.anim.bow"),
                Component.translatable("pmx.anim.crossbow"),
                Component.translatable("pmx.anim.trident"),
                Component.translatable("pmx.anim.shield"),
                Component.translatable("pmx.anim.spyglass")
        )));
        return out;
    }

    private static List<Component> poseLabels(Pose[] poses) {
        List<Component> out = new ArrayList<>(poses.length);
        for (Pose pose : poses) {
            out.add(poseLabel(pose));
        }
        return out;
    }

    private static Component poseLabel(Pose pose) {
        String key = switch (pose) {
            case FALL_FLYING -> "pmx.pose.fall_flying";
            case SLEEPING -> "pmx.pose.sleeping";
            case SWIMMING -> "pmx.pose.swimming";
            case SPIN_ATTACK -> "pmx.pose.spin_attack";
            case CROUCHING -> "pmx.pose.crouching";
            case DYING -> "pmx.pose.dying";
            case SITTING -> "pmx.pose.sitting";
            default -> "pmx.pose.unknown";
        };
        return Component.translatable(key);
    }

    private record Category(Component label, List<Component> items) {}

    private final class PmxStateList extends AbstractSelectionList<PmxStateList.Entry> {
        private final List<Category> categories;
        private int buttonWidth = 90;
        private final int buttonGap = 6;

        private PmxStateList(Minecraft minecraft, int width, int height,
                             int y0, int y1, int itemHeight, List<Category> categories) {
            super(minecraft, width, height, y0, y1, itemHeight);
            this.categories = categories;
            recalcLayout(width);
            buildEntries();
        }

        private void buildEntries() {
            clearEntries();
            addEntry(new HeaderEntry());
            int maxRows = 0;
            for (Category category : categories) {
                if (category.items().size() > maxRows) {
                    maxRows = category.items().size();
                }
            }
            for (int row = 0; row < maxRows; row++) {
                addEntry(new RowEntry(row));
            }
        }

        private void recalcLayout(int width) {
            int columns = categories.size();
            int available = Math.max(0, width - (buttonGap * Math.max(0, columns - 1)));
            int minBtn = 80;
            int maxBtn = 140;
            int btn = columns > 0 ? (available / columns) : minBtn;
            buttonWidth = Math.max(minBtn, Math.min(maxBtn, btn));
        }

        @Override
        protected int getScrollbarPosition() {
            return this.getRight() - 6;
        }

        @Override
        public int getRowWidth() {
            return this.width;
        }

        @Override
        public void updateNarration(@NotNull net.minecraft.client.gui.narration.NarrationElementOutput output) {
        }

        @Override
        protected void renderSelection(@NotNull GuiGraphics graphics, int y, int entryWidth, int entryHeight,
                                       int borderColor, int fillColor) {
            // Selection handled by per-button highlight.
        }

        int separatorX() {
            return this.getRowLeft() + buttonWidth + buttonGap / 2;
        }

        private abstract static class Entry extends AbstractSelectionList.Entry<Entry> {}

        private final class HeaderEntry extends Entry {
            @Override
            public void render(@NotNull GuiGraphics graphics, int index, int y, int x, int width, int height,
                               int mouseX, int mouseY, boolean hovered, float partialTick) {
                for (int i = 0; i < categories.size(); i++) {
                    int bx = x + i * (buttonWidth + buttonGap);
                    Component label = categories.get(i).label();
                    graphics.drawCenteredString(font, label, bx + buttonWidth / 2, y + 5, 0xC0C0C0);
                }
            }
        }

        private final class RowEntry extends Entry {
            private final int rowIndex;
            private int lastX;
            private int lastY;
            private int lastHeight;

            private RowEntry(int rowIndex) {
                this.rowIndex = rowIndex;
            }

            @Override
            public void render(@NotNull GuiGraphics graphics, int index, int y, int x, int width, int height,
                               int mouseX, int mouseY, boolean hovered, float partialTick) {
                lastX = x;
                lastY = y;
                lastHeight = height;
                for (int i = 0; i < categories.size(); i++) {
                    List<Component> items = categories.get(i).items();
                    if (rowIndex >= items.size()) continue;
                    Component label = items.get(rowIndex);
                    int bx = x + i * (buttonWidth + buttonGap);
                    renderButton(graphics, bx, y + 2, buttonWidth, height - 4, label, i,
                            mouseX, mouseY);
                }
            }

            private void renderButton(GuiGraphics graphics, int x, int y, int width, int height,
                                      Component label, int column, int mouseX, int mouseY) {
                boolean over = mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + height;
                boolean selected = isSelected(label, column);
                int fill = selected ? 0x55333333 : (over ? 0x40222222 : 0x20222222);
                int border = selected ? 0xFFFFFFFF : (over ? 0xFFCCCCCC : 0xFF777777);
                graphics.fill(x, y, x + width, y + height, fill);
                graphics.renderOutline(x, y, width, height, border);
                graphics.drawCenteredString(font, label, x + width / 2, y + 5, 0xFFFFFF);
            }

            private boolean isSelected(Component label, int column) {
                if (column == 0) {
                    return selectedPose != null && selectedPose.getString().equals(label.getString());
                }
                return selectedAnim != null && selectedAnim.getString().equals(label.getString());
            }

            @Override
            public boolean mouseClicked(double mouseX, double mouseY, int button) {
                if (mouseY < lastY || mouseY > lastY + lastHeight) return false;
                int startX = lastX;
                for (int i = 0; i < categories.size(); i++) {
                    int bx = startX + i * (buttonWidth + buttonGap);
                    int by = lastY + 2;
                    int bw = buttonWidth;
                    int bh = lastHeight - 4;
                    List<Component> items = categories.get(i).items();
                    if (rowIndex >= items.size()) continue;
                    if (mouseX >= bx && mouseX <= bx + bw && mouseY >= by && mouseY <= by + bh) {
                        Component label = items.get(rowIndex);
                        if (i == 0) {
                            onPickPose(label);
                        } else {
                            onPickAnim(label);
                        }
                        return true;
                    }
                }
                return false;
            }
        }
    }
}
