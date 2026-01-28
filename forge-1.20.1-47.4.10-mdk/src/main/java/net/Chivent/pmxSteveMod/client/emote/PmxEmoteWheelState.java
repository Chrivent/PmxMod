package net.Chivent.pmxSteveMod.client.emote;

public final class PmxEmoteWheelState {
    private static int lastSelectedSlot = -1;

    private PmxEmoteWheelState() {}

    public static int getLastSelectedSlot() {
        return lastSelectedSlot;
    }

    public static void setLastSelectedSlot(int slot) {
        lastSelectedSlot = slot;
    }
}
