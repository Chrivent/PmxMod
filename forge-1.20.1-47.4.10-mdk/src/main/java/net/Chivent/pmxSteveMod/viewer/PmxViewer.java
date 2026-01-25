package net.Chivent.pmxSteveMod.viewer;

import com.mojang.blaze3d.vertex.PoseStack;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.client.renderer.entity.player.PlayerRenderer;
import com.mojang.logging.LogUtils;
import net.Chivent.pmxSteveMod.jni.PmxNative;
import org.slf4j.Logger;
import java.nio.*;

public class PmxViewer {
    private static final Logger LOGGER = LogUtils.getLogger();
    private static final PmxViewer INSTANCE = new PmxViewer();
    public static PmxViewer get() { return INSTANCE; }

    private long handle = 0L;
    private boolean ready = false;
    private float frame = 0f;
    private long lastNanos = -1;

    private ByteBuffer idxBuf;
    private FloatBuffer posBuf;
    private FloatBuffer nrmBuf;
    private FloatBuffer uvBuf;

    private PmxViewer() {}

    public boolean isReady() {
        return ready;
    }

    private void allocateCpuBuffers() {
        int vertexCount = PmxNative.nativeGetVertexCount(handle);
        int indexCount = PmxNative.nativeGetIndexCount(handle);
        int indexElemSize = PmxNative.nativeGetIndexElementSize(handle);

        // indices (ByteBuffer)
        idxBuf = ByteBuffer.allocateDirect(indexCount * indexElemSize).order(ByteOrder.nativeOrder());

        // positions/normals/uvs (FloatBuffer)
        posBuf = ByteBuffer.allocateDirect(vertexCount * 3 * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        nrmBuf = ByteBuffer.allocateDirect(vertexCount * 3 * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        uvBuf  = ByteBuffer.allocateDirect(vertexCount * 2 * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
    }

    private void copyOnce() {
        idxBuf.clear(); posBuf.clear(); nrmBuf.clear(); uvBuf.clear();

        PmxNative.nativeCopyIndices(handle, idxBuf);
        PmxNative.nativeCopyPositions(handle, posBuf);
        PmxNative.nativeCopyNormals(handle, nrmBuf);
        PmxNative.nativeCopyUVs(handle, uvBuf);
    }

    public void init() {
        try {
            LOGGER.info("[PMX] before nativeCreate");
            handle = PmxNative.nativeCreate();
            LOGGER.info("[PMX] after nativeCreate handle={}", handle);

            String dataDir = "C:/Users/Ha Yechan/Desktop/PmxMod/resource/mmd";
            String pmxPath = "D:/예찬/MMD/model/Booth/Chrivent Elf/Chrivent Elf.pmx";

            LOGGER.info("[PMX] before nativeLoadPmx");
            boolean ok = PmxNative.nativeLoadPmx(handle, pmxPath, dataDir);
            LOGGER.info("[PMX] after nativeLoadPmx ok={}", ok);
            if (!ok) {
                ready = false;
                return;
            }

            boolean vmdOk = PmxNative.nativeAddVmd(handle, "D:/예찬/MMD/motion/STAYC - Teddy Bear/STAYC - Teddy Bear/Teddy Bear.vmd");
            LOGGER.info("[PMX] nativeAddVmd ok={}", vmdOk);

            PmxNative.nativeUpdate(handle, 0.0f, 0.0f);

            allocateCpuBuffers();
            copyOnce();

            // 5) 정보 로그
            int vtx = PmxNative.nativeGetVertexCount(handle);
            int idx = PmxNative.nativeGetIndexCount(handle);
            int elem = PmxNative.nativeGetIndexElementSize(handle);

            LOGGER.info("[PMX] vertexCount={} indexCount={} indexElementSize={}", vtx, idx, elem);

            String name = PmxNative.nativeGetModelName(handle);
            String enName = PmxNative.nativeGetEnglishModelName(handle);
            String comment = PmxNative.nativeGetComment(handle);
            String enComment = PmxNative.nativeGetEnglishComment(handle);

            LOGGER.info("[PMX] name='{}' en='{}'", name, enName);
            LOGGER.info("[PMX] comment='{}'", comment != null ? comment.substring(0, Math.min(200, comment.length())) : null);
            LOGGER.info("[PMX] enComment='{}'", enComment != null ? enComment.substring(0, Math.min(200, enComment.length())) : null);

            ready = true;

        } catch (UnsatisfiedLinkError e) {
            LOGGER.error("[PMX] Native link error (missing symbol or load fail): {}", e.getMessage());
            ready = false;
        } catch (Throwable t) {
            LOGGER.error("[PMX] init error", t);
            ready = false;
        }
    }

    public void tick() {
        if (!ready || handle == 0L)
            return;

        long now = System.nanoTime();
        if (lastNanos < 0) lastNanos = now;
        float dt = (now - lastNanos) / 1_000_000_000.0f;
        lastNanos = now;

        frame += dt * 30.0f;
        PmxNative.nativeUpdate(handle, frame, dt);
    }

    public void renderPlayer(AbstractClientPlayer player,
                             PlayerRenderer vanillaRenderer,
                             float partialTick,
                             PoseStack poseStack,
                             MultiBufferSource buffers,
                             int packedLight) {
        // TODO: implement later
    }

    public void onReload() {
        // TODO: implement later
    }

    public void shutdown() {
        if (handle != 0L) {
            try {
                PmxNative.nativeDestroy(handle);
            } catch (Throwable ignored) {}
            handle = 0L;
        }
        ready = false;
    }
}
