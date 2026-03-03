package net.Chivent.pmxSteveMod.client.settings;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import java.io.BufferedWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;
import net.minecraftforge.fml.loading.FMLPaths;

public final class PmxToolSettingsStore {
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    private static final PmxToolSettingsStore INSTANCE = new PmxToolSettingsStore();
    private static final String FILE_NAME = "tool_settings.json";
    private final Path filePath;
    private boolean loaded;
    private StoreData data = new StoreData();
    private final Map<String, ToolOffsets> liveOverrides = new HashMap<>();

    public static PmxToolSettingsStore get() {
        return INSTANCE;
    }

    private PmxToolSettingsStore() {
        Path dir = FMLPaths.CONFIGDIR.get().resolve("pmx_steve_mod");
        this.filePath = dir.resolve(FILE_NAME);
    }

    public synchronized ToolOffsets get(Path modelPath) {
        ensureLoaded();
        if (modelPath == null) return new ToolOffsets();
        String key = modelPath.toString();
        ToolOffsets live = liveOverrides.get(key);
        if (live != null) return live.copy();
        ToolOffsets offsets = data.models.get(key);
        return offsets != null ? offsets.copy() : new ToolOffsets();
    }

    public synchronized void save(Path modelPath, ToolOffsets offsets) {
        if (modelPath == null || offsets == null) return;
        ensureLoaded();
        data.models.put(modelPath.toString(), offsets.copy());
        writeFile();
    }

    public synchronized void setLive(Path modelPath, ToolOffsets offsets) {
        if (modelPath == null || offsets == null) return;
        liveOverrides.put(modelPath.toString(), offsets.copy());
    }

    public synchronized void clearLive(Path modelPath) {
        if (modelPath == null) return;
        liveOverrides.remove(modelPath.toString());
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

    private static final class StoreData {
        private final Map<String, ToolOffsets> models = new HashMap<>();
    }

    public static final class ToolOffsets {
        public ToolCategoryOffsets handheld = new ToolCategoryOffsets();
        public ToolCategoryOffsets rod = new ToolCategoryOffsets();
        public ToolCategoryOffsets bow = new ToolCategoryOffsets();
        public ToolCategoryOffsets crossbow = new ToolCategoryOffsets();
        public ToolCategoryOffsets shield = new ToolCategoryOffsets();
        public ToolCategoryOffsets trident = new ToolCategoryOffsets();
        public ToolCategoryOffsets spyglass = new ToolCategoryOffsets();
        public ToolCategoryOffsets goatHorn = new ToolCategoryOffsets();
        public ToolCategoryOffsets brush = new ToolCategoryOffsets();

        private ToolOffsets copy() {
            ToolOffsets out = new ToolOffsets();
            out.handheld = handheld.copy();
            out.rod = rod.copy();
            out.bow = bow.copy();
            out.crossbow = crossbow.copy();
            out.shield = shield.copy();
            out.trident = trident.copy();
            out.spyglass = spyglass.copy();
            out.goatHorn = goatHorn.copy();
            out.brush = brush.copy();
            return out;
        }
    }

    public static final class ToolCategoryOffsets {
        public ToolOffset base = new ToolOffset();
        public ToolOffset alt = new ToolOffset();

        private ToolCategoryOffsets copy() {
            ToolCategoryOffsets out = new ToolCategoryOffsets();
            out.base = base.copy();
            out.alt = alt.copy();
            return out;
        }
    }

    public static final class ToolOffset {
        public float posX;
        public float posY;
        public float posZ;
        public float rotX;
        public float rotY;
        public float rotZ;

        private ToolOffset copy() {
            ToolOffset out = new ToolOffset();
            out.posX = posX;
            out.posY = posY;
            out.posZ = posZ;
            out.rotX = rotX;
            out.rotY = rotY;
            out.rotZ = rotZ;
            return out;
        }
    }
}
