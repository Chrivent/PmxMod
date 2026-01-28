package net.Chivent.pmxSteveMod.client.settings;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import net.minecraftforge.fml.loading.FMLPaths;

public final class PmxModelSettingsStore {
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    private static final PmxModelSettingsStore INSTANCE = new PmxModelSettingsStore();
    private static final String FILE_NAME = "model_settings.json";
    private final Path filePath;
    private boolean loaded;
    private StoreData data = new StoreData();

    public static PmxModelSettingsStore get() {
        return INSTANCE;
    }

    private PmxModelSettingsStore() {
        Path dir = FMLPaths.CONFIGDIR.get().resolve("pmx_steve_mod");
        this.filePath = dir.resolve(FILE_NAME);
    }

    public synchronized List<RowData> load(Path modelPath) {
        if (modelPath == null) return List.of();
        ensureLoaded();
        String key = modelPath.toString();
        List<RowData> rows = data.models.get(key);
        if (rows == null) return List.of();
        return new ArrayList<>(rows);
    }

    public synchronized String getIdleMotion(Path modelPath) {
        if (modelPath == null) return "";
        ensureLoaded();
        List<RowData> rows = data.models.get(modelPath.toString());
        if (rows == null) return "";
        for (RowData row : rows) {
            if (row.slotIndex == -2 && row.motion != null && !row.motion.isBlank()) {
                return row.motion;
            }
        }
        return "";
    }

    public synchronized void save(Path modelPath, List<RowData> rows) {
        if (modelPath == null) return;
        ensureLoaded();
        data.models.put(modelPath.toString(), new ArrayList<>(rows));
        writeFile();
    }

    private void ensureLoaded() {
        if (loaded) return;
        loaded = true;
        if (!Files.exists(filePath)) return;
        try (BufferedReader reader = Files.newBufferedReader(filePath)) {
            StoreData loadedData = GSON.fromJson(reader, StoreData.class);
            if (loadedData != null) {
                data = loadedData;
            }
        } catch (Exception ignored) {
        }
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

    private static final class StoreData {
        private final Map<String, List<RowData>> models = new HashMap<>();
    }

    public static final class RowData {
        public String name = "";
        public boolean custom;
        public int slotIndex = -1;
        public boolean motionLoop;
        public boolean stopOnMove;
        public boolean cameraLock;
        public String motion = "";
        public String camera = "";
        public String music = "";
    }
}
