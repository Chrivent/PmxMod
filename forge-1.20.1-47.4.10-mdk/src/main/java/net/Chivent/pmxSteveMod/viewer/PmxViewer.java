package net.Chivent.pmxSteveMod.viewer;

import com.mojang.blaze3d.vertex.PoseStack;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.client.renderer.entity.player.PlayerRenderer;
import com.mojang.logging.LogUtils;
import net.Chivent.pmxSteveMod.jni.PmxNative;
import org.slf4j.Logger;
import java.nio.*;

import com.mojang.blaze3d.vertex.VertexConsumer;
import com.mojang.math.Axis;
import net.minecraft.client.renderer.RenderType;
import net.minecraft.util.Mth;
import org.joml.Matrix3f;
import org.joml.Matrix4f;

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
    private ByteBuffer posBB;
    private ByteBuffer nrmBB;
    private ByteBuffer uvBB;

    private PmxViewer() {}

    public boolean isReady() {
        return ready;
    }

    private void allocateCpuBuffers() {
        int vertexCount = PmxNative.nativeGetVertexCount(handle);
        int indexCount = PmxNative.nativeGetIndexCount(handle);
        int indexElemSize = PmxNative.nativeGetIndexElementSize(handle);

        idxBuf = ByteBuffer.allocateDirect(indexCount * indexElemSize).order(ByteOrder.nativeOrder());

        posBB = ByteBuffer.allocateDirect(vertexCount * 3 * 4).order(ByteOrder.nativeOrder());
        nrmBB = ByteBuffer.allocateDirect(vertexCount * 3 * 4).order(ByteOrder.nativeOrder());
        uvBB  = ByteBuffer.allocateDirect(vertexCount * 2 * 4).order(ByteOrder.nativeOrder());

        posBuf = posBB.asFloatBuffer();
        nrmBuf = nrmBB.asFloatBuffer();
        uvBuf  = uvBB.asFloatBuffer();
    }

    private void copyOnce() {
        idxBuf.clear();
        posBB.clear();
        nrmBB.clear();
        uvBB.clear();

        PmxNative.nativeCopyIndices(handle, idxBuf);
        PmxNative.nativeCopyPositions(handle, posBB);
        PmxNative.nativeCopyNormals(handle, nrmBB);
        PmxNative.nativeCopyUVs(handle, uvBB);
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
        if (!ready || handle == 0L) return;
        if (idxBuf == null || posBuf == null) return;

        copyOnce();

        poseStack.pushPose();

        float yRot = Mth.lerp(partialTick, player.yRotO, player.getYRot());
        poseStack.mulPose(Axis.YP.rotationDegrees(180.0f - yRot));

        float scale = 0.15f;
        poseStack.scale(scale, scale, scale);

        Matrix4f pose = poseStack.last().pose();
        Matrix3f normalMat = poseStack.last().normal();

        VertexConsumer vc = buffers.getBuffer(RenderType.lines());

        idxBuf.rewind();
        int indexCount = PmxNative.nativeGetIndexCount(handle);
        int elemSize = PmxNative.nativeGetIndexElementSize(handle);

        posBuf.rewind();
        nrmBuf.rewind();

        int triCount = indexCount / 3;
        for (int t = 0; t < triCount; t++) {
            int i0 = readIndex(idxBuf, elemSize, t * 3);
            int i1 = readIndex(idxBuf, elemSize, t * 3 + 1);
            int i2 = readIndex(idxBuf, elemSize, t * 3 + 2);

            int vtxCount = posBuf.capacity() / 3;
            if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= vtxCount || i1 >= vtxCount || i2 >= vtxCount)
                continue;

            float x0 = posBuf.get(i0 * 3), y0 = posBuf.get(i0 * 3 + 1), z0 = posBuf.get(i0 * 3 + 2);
            float x1 = posBuf.get(i1 * 3), y1 = posBuf.get(i1 * 3 + 1), z1 = posBuf.get(i1 * 3 + 2);
            float x2 = posBuf.get(i2 * 3), y2 = posBuf.get(i2 * 3 + 1), z2 = posBuf.get(i2 * 3 + 2);

            addLine(vc, pose, normalMat, x0, y0, z0, x1, y1, z1);
            addLine(vc, pose, normalMat, x1, y1, z1, x2, y2, z2);
            addLine(vc, pose, normalMat, x2, y2, z2, x0, y0, z0);
        }

        poseStack.popPose();
    }

    private static int readIndex(ByteBuffer buf, int elemSize, int idx) {
        int bytePos = idx * elemSize;
        if (bytePos < 0 || bytePos + elemSize > buf.capacity()) return -1;

        return switch (elemSize) {
            case 1 -> (buf.get(bytePos) & 0xFF);
            case 2 -> (buf.getShort(bytePos) & 0xFFFF);
            case 4 -> buf.getInt(bytePos);
            default -> -1;
        };
    }

    // RenderType.lines()는 POSITION_COLOR_NORMAL 포맷이라 normal을 같이 넣어줌
    private static void addLine(VertexConsumer vc,
                                Matrix4f pose,
                                Matrix3f normalMat,
                                float ax, float ay, float az,
                                float bx, float by, float bz) {
        // 그냥 흰색 와이어
        int r = 255, g = 255, b = 255, a = 255;

        // 라인 normal은 대충 위쪽으로 (빛 계산 크게 신경 안 쓰는 용도)
        float nx = 0f, ny = 1f, nz = 0f;

        vc.vertex(pose, ax, ay, az).color(r, g, b, a).normal(normalMat, nx, ny, nz).endVertex();
        vc.vertex(pose, bx, by, bz).color(r, g, b, a).normal(normalMat, nx, ny, nz).endVertex();
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
