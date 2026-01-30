package net.Chivent.pmxSteveMod.client.util;

import com.mojang.logging.LogUtils;
import net.minecraftforge.fml.ModList;
import org.slf4j.Logger;

public final class PmxShaderPackUtil {
    private static final Logger LOGGER = LogUtils.getLogger();
    private static final long SHADER_PACK_CHECK_INTERVAL_NANOS = 500_000_000L;
    private static boolean cachedShaderPackActive = false;
    private static long lastShaderPackCheckNanos = 0L;
    private static Boolean cachedOculusLoaded = null;

    private PmxShaderPackUtil() {}

    public static boolean isOculusLoaded() {
        if (cachedOculusLoaded == null) {
            cachedOculusLoaded = ModList.get().isLoaded("oculus");
            LOGGER.debug("[PMX] Oculus loaded: {}", cachedOculusLoaded);
        }
        return cachedOculusLoaded;
    }

    public static boolean isShaderPackActive() {
        long now = System.nanoTime();
        if (now - lastShaderPackCheckNanos < SHADER_PACK_CHECK_INTERVAL_NANOS) {
            return cachedShaderPackActive;
        }
        lastShaderPackCheckNanos = now;
        boolean next;
        try {
            Class<?> api = Class.forName("net.irisshaders.iris.api.v0.IrisApi");
            Object inst = api.getMethod("getInstance").invoke(null);
            Object active = api.getMethod("isShaderPackInUse").invoke(inst);
            next = active instanceof Boolean b && b;
        } catch (ReflectiveOperationException e) {
            LOGGER.debug("[PMX] Iris/Oculus API not available: {}", "net.irisshaders.iris.api.v0.IrisApi");
            next = false;
        } catch (Exception e) {
            LOGGER.debug("[PMX] Iris/Oculus API error: {}", "net.irisshaders.iris.api.v0.IrisApi");
            next = false;
        }
        if (next != cachedShaderPackActive) {
            LOGGER.debug("[PMX] Shader pack active changed: {}", next);
        }
        cachedShaderPackActive = next;
        return cachedShaderPackActive;
    }
}
