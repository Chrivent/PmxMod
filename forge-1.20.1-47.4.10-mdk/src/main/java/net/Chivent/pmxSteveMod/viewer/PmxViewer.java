package net.Chivent.pmxSteveMod.viewer;

import net.minecraft.client.Minecraft;
import net.minecraftforge.fml.loading.FMLPaths;

import java.nio.file.Files;
import java.nio.file.Path;

public class PmxViewer {
    private static final PmxViewer INSTANCE = new PmxViewer();
    private final PmxInstance instance = new PmxInstance();
    private boolean showPmx = false;
    private Path userModelDir;
    private Path userMotionDir;
    private Path userCameraDir;
    private Path selectedModelPath;

    public static PmxViewer get() { return INSTANCE; }
    public PmxInstance instance() { return instance; }
    public boolean isPmxVisible() { return showPmx; }
    public void setPmxVisible(boolean visible) { showPmx = visible; }
    public Path getSelectedModelPath() { return selectedModelPath; }

    private PmxViewer() {}

    public void init() {
        showPmx = false;
        selectDefaultModelIfMissing();
        instance.init(selectedModelPath);
    }

    public void shutdown() {
        showPmx = false;
        instance.shutdown();
    }

    public void setSelectedModelPath(Path path) {
        selectedModelPath = path;
    }

    public void reloadSelectedModel() {
        instance.shutdown();
        instance.init(selectedModelPath);
    }

    public Path getUserModelDir() {
        if (userModelDir != null) return userModelDir;
        userModelDir = ensureUserDir("pmx_models");
        return userModelDir;
    }

    public Path getUserMotionDir() {
        if (userMotionDir != null) return userMotionDir;
        userMotionDir = ensureUserDir("pmx_motions");
        return userMotionDir;
    }

    public Path getUserCameraDir() {
        if (userCameraDir != null) return userCameraDir;
        userCameraDir = ensureUserDir("pmx_cameras");
        return userCameraDir;
    }

    private void selectDefaultModelIfMissing() {
        if (selectedModelPath != null && Files.exists(selectedModelPath)) return;
        Path dir = getUserModelDir();
        try (var stream = Files.list(dir)) {
            selectedModelPath = stream
                    .filter(p -> p.toString().toLowerCase().endsWith(".pmx"))
                    .findFirst()
                    .orElse(null);
        } catch (Exception ignored) {
            selectedModelPath = null;
        }
    }

    private Path ensureUserDir(String name) {
        Path base;
        try {
            base = FMLPaths.GAMEDIR.get();
        } catch (Exception ignored) {
            base = Minecraft.getInstance().gameDirectory.toPath();
        }
        Path dir = base.resolve(name);
        try {
            Files.createDirectories(dir);
        } catch (Exception ignored) {
            return dir;
        }
        return dir;
    }
}
