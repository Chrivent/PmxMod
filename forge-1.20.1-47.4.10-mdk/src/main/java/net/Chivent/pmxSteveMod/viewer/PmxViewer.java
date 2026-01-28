package net.Chivent.pmxSteveMod.viewer;

import net.minecraft.client.Minecraft;
import net.minecraftforge.fml.loading.FMLPaths;
import net.Chivent.pmxSteveMod.client.settings.PmxModelSettingsStore;

import java.nio.file.Files;
import java.nio.file.Path;

public class PmxViewer {
    private static final PmxViewer INSTANCE = new PmxViewer();
    private final PmxInstance instance = new PmxInstance();
    private boolean showPmx = false;
    private Path userModelDir;
    private Path userMotionDir;
    private Path userCameraDir;
    private Path userMusicDir;
    private Path selectedModelPath;
    private int currentSlotIndex = -1;
    private boolean currentMotionSlot = false;

    public static PmxViewer get() { return INSTANCE; }
    public PmxInstance instance() { return instance; }
    public boolean isPmxVisible() { return showPmx; }
    public void setPmxVisible(boolean visible) {
        showPmx = visible;
        if (!visible) {
            instance.stopMusic();
            currentSlotIndex = -1;
            currentMotionSlot = false;
        }
    }
    public Path getSelectedModelPath() { return selectedModelPath; }

    private PmxViewer() {}

    public void init() {
        showPmx = false;
        selectDefaultModelIfMissing();
        instance.init(selectedModelPath);
        instance.markBlendNext();
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
        instance.markBlendNext();
    }

    public void handleMotionEnd() {
        if (!showPmx) return;
        if (instance.consumeMotionEnded()) {
            playIdleOrDefault();
        }
    }

    public void playIdleOrDefault() {
        currentSlotIndex = -1;
        currentMotionSlot = false;
        Path modelPath = selectedModelPath;
        PmxModelSettingsStore.RowData idle = PmxModelSettingsStore.get().getIdleRow(modelPath);
        if (idle == null || idle.motion == null || idle.motion.isBlank()) {
            instance.resetToDefaultPose();
            return;
        }
        Path motionDir = getUserMotionDir();
        Path vmdPath = motionDir.resolve(idle.motion);
        Path musicPath = null;
        if (idle.music != null && !idle.music.isBlank()) {
            Path musicDir = getUserMusicDir();
            musicPath = musicDir.resolve(idle.music);
        }
        Path cameraPath = null;
        if (idle.camera != null && !idle.camera.isBlank()) {
            Path cameraDir = getUserCameraDir();
            cameraPath = cameraDir.resolve(idle.camera);
        }
        instance.playMotion(vmdPath, musicPath, cameraPath, idle.motionLoop, false);
    }

    public void setCurrentSlotMotion(int slotIndex) {
        currentSlotIndex = slotIndex;
        currentMotionSlot = slotIndex >= 0;
    }

    public int getCurrentSlotIndex() {
        return currentMotionSlot ? currentSlotIndex : -1;
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

    public Path getUserMusicDir() {
        if (userMusicDir != null) return userMusicDir;
        userMusicDir = ensureUserDir("pmx_music");
        return userMusicDir;
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
