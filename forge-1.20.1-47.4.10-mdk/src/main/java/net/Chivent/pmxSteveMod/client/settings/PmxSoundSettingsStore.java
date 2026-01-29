package net.Chivent.pmxSteveMod.client.settings;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import java.io.BufferedWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import net.minecraftforge.fml.loading.FMLPaths;

public final class PmxSoundSettingsStore {
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    private static final PmxSoundSettingsStore INSTANCE = new PmxSoundSettingsStore();
    private static final String FILE_NAME = "sound_settings.json";
    private final Path filePath;
    private boolean loaded;
    private StoreData data = new StoreData();

    public static PmxSoundSettingsStore get() {
        return INSTANCE;
    }

    private PmxSoundSettingsStore() {
        Path dir = FMLPaths.CONFIGDIR.get().resolve("pmx_steve_mod");
        this.filePath = dir.resolve(FILE_NAME);
    }

    public synchronized float getMusicVolume() {
        ensureLoaded();
        return clamp01(data.musicVolume);
    }

    public synchronized void setMusicVolume(float volume) {
        ensureLoaded();
        data.musicVolume = clamp01(volume);
        writeFile();
    }

    private void ensureLoaded() {
        if (loaded) return;
        loaded = true;
        data = PmxJsonStoreUtil.loadJson(GSON, filePath, StoreData.class, data);
    }

    private void writeFile() {
        try {
            Files.createDirectories(filePath.getParent());
            try (BufferedWriter writer = Files.newBufferedWriter(filePath)) {
                GSON.toJson(data, writer);
            }
        } catch (Exception ignored) {
        }
    }

    private float clamp01(float value) {
        if (value < 0.0f) return 0.0f;
        return Math.min(value, 1.0f);
    }

    private static final class StoreData {
        private float musicVolume = 1.0f;
    }
}
