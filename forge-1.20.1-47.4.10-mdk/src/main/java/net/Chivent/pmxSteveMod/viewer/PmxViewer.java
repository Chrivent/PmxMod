package net.Chivent.pmxSteveMod.viewer;

public class PmxViewer {
    private static final PmxViewer INSTANCE = new PmxViewer();
    private final PmxInstance instance = new PmxInstance();
    private boolean showPmx = false;

    public static PmxViewer get() { return INSTANCE; }
    public PmxInstance instance() { return instance; }
    public boolean isPmxVisible() { return showPmx; }
    public void setPmxVisible(boolean visible) { showPmx = visible; }

    private PmxViewer() {}

    public void init() {
        showPmx = false;
        instance.init();
    }

    public void shutdown() {
        showPmx = false;
        instance.shutdown();
    }
}
