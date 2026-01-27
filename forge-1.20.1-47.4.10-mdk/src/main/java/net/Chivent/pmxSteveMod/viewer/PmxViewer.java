package net.Chivent.pmxSteveMod.viewer;

public class PmxViewer {
    private static final PmxViewer INSTANCE = new PmxViewer();
    private final PmxInstance instance = new PmxInstance();

    public static PmxViewer get() { return INSTANCE; }
    public PmxInstance instance() { return instance; }

    private PmxViewer() {}

    public boolean isReady() { return instance.isReady(); }

    public void init() {
        instance.init();
    }

    public void shutdown() {
        instance.shutdown();
    }
}
