package net.Chivent.pmxSteveMod.client.settings;

import com.google.gson.Gson;
import java.io.BufferedReader;
import java.nio.file.Files;
import java.nio.file.Path;

final class PmxJsonStoreUtil {
    private PmxJsonStoreUtil() {}

    static <T> T loadJson(Gson gson, Path filePath, Class<T> type, T fallback) {
        if (filePath == null || !Files.exists(filePath)) return fallback;
        try (BufferedReader reader = Files.newBufferedReader(filePath)) {
            T loaded = gson.fromJson(reader, type);
            return loaded != null ? loaded : fallback;
        } catch (Exception ignored) {
            return fallback;
        }
    }
}
