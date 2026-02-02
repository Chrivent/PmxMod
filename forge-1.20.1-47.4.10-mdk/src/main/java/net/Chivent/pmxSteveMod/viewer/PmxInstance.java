package net.Chivent.pmxSteveMod.viewer;

import com.mojang.logging.LogUtils;
import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.viewer.controllers.PmxCameraController;
import net.Chivent.pmxSteveMod.viewer.controllers.PmxMusicController;
import net.Chivent.pmxSteveMod.viewer.controllers.PmxPlaybackController;
import net.Chivent.pmxSteveMod.jni.PmxNative;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.resources.ResourceLocation;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.LivingEntity;
import org.slf4j.Logger;

import java.io.InputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;

public class PmxInstance {
    private static final Logger LOGGER = LogUtils.getLogger();
    private static Path cachedToonDir;

    private long handle = 0L;
    private boolean ready = false;
    private long modelVersion = 0L;
    private Path currentPmxPath;
    private final PmxPlaybackController playbackController = new PmxPlaybackController();
    private final PmxMusicController musicController = new PmxMusicController();
    private final PmxCameraController cameraController = new PmxCameraController();
    private static final String HEAD_BONE_NAME = "頭";
    private boolean headBoneChecked = false;
    private boolean headBoneAvailable = false;

    private ByteBuffer idxBuf;
    private ByteBuffer posBuf;
    private ByteBuffer nrmBuf;
    private ByteBuffer uvBuf;

    private Path pmxBaseDir = null;

    private SubmeshInfo[] submeshes;
    private MaterialInfo[] materials;

    private final ByteBuffer tmp3f = ByteBuffer.allocateDirect(3 * 4).order(ByteOrder.nativeOrder());
    private final ByteBuffer tmp4f = ByteBuffer.allocateDirect(4 * 4).order(ByteOrder.nativeOrder());

    private boolean indicesCopiedOnce = false;

    public boolean isReady() { return ready; }
    public long handle() { return handle; }
    public long modelVersion() { return modelVersion; }

    public ByteBuffer idxBuf() { return idxBuf; }
    public ByteBuffer posBuf() { return posBuf; }
    public ByteBuffer nrmBuf() { return nrmBuf; }
    public ByteBuffer uvBuf()  { return uvBuf;  }

    public SubmeshInfo[] submeshes() { return submeshes; }

    public MaterialInfo material(int id) {
        if (materials == null || id < 0 || id >= materials.length) return null;
        return materials[id];
    }

    public Path pmxBaseDir() { return pmxBaseDir; }

    public ModelInfo getModelInfo() {
        if (!ready || handle == 0L) return null;
        try {
            return new ModelInfo(
                    cleanText(PmxNative.nativeGetModelName(handle)),
                    cleanText(PmxNative.nativeGetEnglishModelName(handle)),
                    cleanText(PmxNative.nativeGetComment(handle)),
                    cleanText(PmxNative.nativeGetEnglishComment(handle))
            );
        } catch (Throwable ignored) {
            return null;
        }
    }

    public void init(Path pmxPath) {
        currentPmxPath = pmxPath;
        try {
            handle = PmxNative.nativeCreate();

            if (pmxPath == null || !Files.exists(pmxPath)) {
                ready = false;
                if (handle != 0L) {
                    try { PmxNative.nativeDestroy(handle); } catch (Throwable ignored) {}
                    handle = 0L;
                }
                return;
            }
            pmxBaseDir = pmxPath.getParent();
            Path dataDirPath = ensureToonDir();
            String dataDir = dataDirPath != null ? dataDirPath.toString() : "";

            boolean ok = PmxNative.nativeLoadPmx(handle, pmxPath.toString(), dataDir);
            if (!ok) { ready = false; return; }

            PmxNative.nativeSyncPhysics(handle, 0.0f);
            PmxNative.nativeUpdate(handle, 0.0f, 0.0f);

            allocateCpuBuffers();

            copyIndicesOnce();
            copyDynamicVertices();

            buildMaterials();
            buildSubmeshInfos();

            modelVersion++;
            ready = true;
            playbackController.resetAll();
            resetSyncState(0f, 0f);
            headBoneChecked = false;
            headBoneAvailable = false;
        } catch (Throwable t) {
            LOGGER.error("[PMX] init error", t);
            ready = false;
        }
    }

    public void shutdown() {
        if (handle != 0L) {
            musicController.stopPlayback(handle);
            try { PmxNative.nativeDestroy(handle); } catch (Throwable ignored) {}
            handle = 0L;
        }
        ready = false;
        currentPmxPath = null;
        playbackController.resetAll();
        resetSyncState(0f, 0f);

        indicesCopiedOnce = false;
        headBoneChecked = false;
        headBoneAvailable = false;
    }

    public void tickRender() {
        if (!ready || handle == 0L) return;

        applyHeadAdditiveRotation();

        PmxPlaybackController.Tick baseTick = playbackController.tickBase();
        float dt = baseTick.dt();
        float frame = baseTick.frame();

        PmxMusicController.MusicTickResult musicTick = musicController.updateFrame(handle, frame, dt);
        frame = musicTick.frame();
        dt = musicTick.dt();
        playbackController.setFrame(frame);

        float evalFrame = playbackController.frame();
        if (musicController.useMusicSync()
                && musicController.isMusicLonger()
                && playbackController.currentMotionEndFrame() > 0.0f
                && frame >= playbackController.currentMotionEndFrame()) {
            evalFrame = playbackController.currentMotionEndFrame();
            dt = 0.0f;
        }

        float physicsDt = playbackController.computePhysicsDt(dt);

        float endFrame = playbackController.currentMotionEndFrame();
        if (musicController.isMusicLonger() && musicController.currentMusicEndFrame() > 0.0f) {
            endFrame = musicController.currentMusicEndFrame();
        }
        playbackController.handleEndFrame(endFrame, () -> musicController.onMotionLoop(handle));

        if (musicController.isActive()
                && musicController.isMusicSync()
                && !musicController.isMusicLonger()
                && musicController.currentMusicEndFrame() > 0.0f
                && frame >= musicController.currentMusicEndFrame()) {
            musicController.stopPlayback(handle);
        }

        cameraController.update(handle, evalFrame);
        PmxNative.nativeUpdate(handle, evalFrame, physicsDt);
    }

    private void applyHeadAdditiveRotation() {
        Minecraft mc = Minecraft.getInstance();
        AbstractClientPlayer player = mc.player;
        if (player == null) return;
        if (!headBoneChecked) {
            headBoneAvailable = PmxNative.nativeHasBone(handle, HEAD_BONE_NAME);
            headBoneChecked = true;
            if (!headBoneAvailable) {
                LOGGER.warn("[PMX] head bone '{}' not found. Head tracking disabled.", HEAD_BONE_NAME);
                return;
            }
        } else if (!headBoneAvailable) {
            return;
        }
        float partialTick = mc.getFrameTime();
        float wrappedDiff = getWrappedDiff(partialTick, player);
        float pitch = Mth.lerp(partialTick, player.xRotO, player.getXRot());
        float yaw = -wrappedDiff;
        PmxNative.nativeSetBoneRotationAdditive(handle, HEAD_BONE_NAME, pitch, yaw, 0.0f);
    }

    private static float getWrappedDiff(float partialTick, AbstractClientPlayer player) {
        float bodyYaw = Mth.rotLerp(partialTick, player.yBodyRotO, player.yBodyRot);
        float headYaw = Mth.rotLerp(partialTick, player.yHeadRotO, player.yHeadRot);
        float yawDiff = headYaw - bodyYaw;
        if (player.isPassenger() && player.getVehicle() instanceof LivingEntity living) {
            float vehicleBodyYaw = Mth.rotLerp(partialTick, living.yBodyRotO, living.yBodyRot);
            yawDiff = headYaw - vehicleBodyYaw;
            float clamped = Mth.wrapDegrees(yawDiff);
            if (clamped < -85.0f) clamped = -85.0f;
            if (clamped > 85.0f) clamped = 85.0f;
            bodyYaw = headYaw - clamped;
            if (clamped * clamped > 2500.0f) {
                bodyYaw += clamped * 0.2f;
            }
            yawDiff = headYaw - bodyYaw;
        }
        float wrappedDiff = Mth.wrapDegrees(yawDiff);
        if (wrappedDiff < -85.0f) wrappedDiff = -85.0f;
        if (wrappedDiff > 85.0f) wrappedDiff = 85.0f;
        return wrappedDiff;
    }

    public void syncCpuBuffersForRender() {
        if (!ready || handle == 0L) return;
        copyIndicesOnce();
        if (posBuf == null || nrmBuf == null || uvBuf == null) return;
        copyDynamicVertices();
    }

    public void playMotion(Path vmdPath, Path musicPath, Path cameraPath, boolean loop, boolean musicSync) {
        if (vmdPath == null || !Files.exists(vmdPath)) return;
        Path pmxPath = currentPmxPath;
        if (pmxPath == null) return;
        if (!ready || handle == 0L) {
            init(pmxPath);
        }
        if (!ready || handle == 0L) return;
        try {
            musicController.resetForNewMotion(musicSync);
            cameraController.reset();

            if (cameraPath != null && Files.exists(cameraPath)) {
                Path safeCamera = toSafePath(cameraPath, "camera_cache");
                try {
                    Path sourceCamera = cameraPath.toAbsolutePath().normalize();
                    cameraController.load(handle, sourceCamera, safeCamera);
                } catch (Exception ignored) {
                    cameraController.load(handle, cameraPath, safeCamera);
                }
            } else {
                cameraController.clear(handle);
            }

            Path safePath = toSafePath(vmdPath, "motion_cache");
            float blendSeconds = playbackController.shouldBlendNext() ? 0.3f : 0.0f;
            playbackController.clearForceBlend();
            boolean started = false;
            try {
                started = PmxNative.nativeStartVmdBlend(handle, safePath.toString(), blendSeconds);
            } catch (UnsatisfiedLinkError e) {
                LOGGER.warn("[PMX] nativeStartVmdBlend missing (rebuild native DLL). Falling back to nativeAddVmd.");
            }
            if (!started) {
                if (!PmxNative.nativeAddVmd(handle, safePath.toString())) {
                    return;
                }
            }
            playbackController.resetFrameClock();
            if (!playbackController.hasMotion()) {
                PmxNative.nativeSyncPhysics(handle, 0.0f);
            }
            PmxNative.nativeUpdate(handle, 0.0f, 0.0f);
            try {
                playbackController.onMotionStarted(vmdPath.toAbsolutePath().normalize(), loop, blendSeconds);
            } catch (Exception ignored) {
                playbackController.onMotionStarted(vmdPath, loop, blendSeconds);
            }
            playbackController.setMotionEndFrame(PmxNative.nativeGetMotionMaxFrame(handle));

            if (musicPath != null && Files.exists(musicPath)) {
                Path safeMusic = toSafePath(musicPath, "music_cache");
                try {
                    Path sourceMusic = musicPath.toAbsolutePath().normalize();
                    musicController.start(handle, safeMusic, sourceMusic, loop, musicSync, playbackController.currentMotionEndFrame());
                } catch (Exception ignored) {
                    musicController.start(handle, safeMusic, musicPath, loop, musicSync, playbackController.currentMotionEndFrame());
                }
            } else {
                musicController.stopPlayback(handle);
            }
        } catch (Throwable t) {
            LOGGER.warn("[PMX] failed to play motion {}", vmdPath, t);
        }
    }

    public boolean hasCamera() { return cameraController.isActive(); }
    public Path getCurrentMotionPath() { return playbackController.currentMotionPath(); }
    public Path getCurrentMusicPath() { return musicController.getCurrentMusicSourcePath(); }
    public Path getCurrentCameraPath() { return cameraController.getCurrentCameraPath(); }
    public boolean consumeMotionEnded() {
        return playbackController.consumeMotionEnded();
    }

    public void markBlendNext() {
        playbackController.markBlendNext();
    }
    public float camInterestX() { return cameraController.camInterestX(); }
    public float camInterestY() { return cameraController.camInterestY(); }
    public float camInterestZ() { return cameraController.camInterestZ(); }
    public float camRotX() { return cameraController.camRotX(); }
    public float camRotY() { return cameraController.camRotY(); }
    public float camRotZ() { return cameraController.camRotZ(); }
    public float camDistance() { return cameraController.camDistance(); }
    public float camFov() { return cameraController.camFov(); }

    public void stopMusic() {
        musicController.stopPlayback(handle);
    }

    public void setMusicVolume(float volume) {
        musicController.setVolume(handle, volume);
    }

    public void resetToDefaultPose() {
        Path pmxPath = currentPmxPath;
        if (pmxPath == null) return;
        if (!ready || handle == 0L) {
            init(pmxPath);
            return;
        }
        musicController.stopPlayback(handle);
        cameraController.clear(handle);
        boolean started = false;
        try {
            started = PmxNative.nativeStartDefaultPoseBlend(handle, 0.3f);
        } catch (UnsatisfiedLinkError e) {
            LOGGER.warn("[PMX] nativeStartDefaultPoseBlend missing (rebuild native DLL). Falling back to reset.");
        }
        if (!started) {
            shutdown();
            init(pmxPath);
            playbackController.markBlendNext();
            return;
        }
        playbackController.resetAll();
        playbackController.markBlendNext();
        resetSyncState(0.3f, 0.3f);
    }

    private void resetSyncState(float physicsHold, float physicsDuration) {
        musicController.reset();
        cameraController.reset();
        playbackController.resetPhysicsBlend(physicsHold, physicsDuration);
    }

    private Path toSafePath(Path src, String cacheDirName) throws IOException {
        String name = src.getFileName().toString();
        String safeName = name.replaceAll("[^A-Za-z0-9._-]", "_");
        if (safeName.isBlank()) {
            String ext = "";
            int dot = name.lastIndexOf('.');
            if (dot >= 0 && dot < name.length() - 1) {
                ext = name.substring(dot);
            }
            safeName = ext.isBlank() ? "file.dat" : "file" + ext;
        }
        Path outDir = Paths.get(System.getProperty("java.io.tmpdir"), "pmx_steve_mod", cacheDirName);
        Files.createDirectories(outDir);
        Path outFile = outDir.resolve(safeName);
        Files.copy(src, outFile, StandardCopyOption.REPLACE_EXISTING);
        return outFile;
    }

    private void allocateCpuBuffers() {
        int vertexCount = PmxNative.nativeGetVertexCount(handle);
        int indexCount  = PmxNative.nativeGetIndexCount(handle);
        int elemSize    = PmxNative.nativeGetIndexElementSize(handle);

        idxBuf = ByteBuffer.allocateDirect(indexCount * elemSize).order(ByteOrder.nativeOrder());
        posBuf = ByteBuffer.allocateDirect(vertexCount * 3 * 4).order(ByteOrder.nativeOrder());
        nrmBuf = ByteBuffer.allocateDirect(vertexCount * 3 * 4).order(ByteOrder.nativeOrder());
        uvBuf  = ByteBuffer.allocateDirect(vertexCount * 2 * 4).order(ByteOrder.nativeOrder());
    }

    private void copyIndicesOnce() {
        if (indicesCopiedOnce) return;
        if (idxBuf == null) return;

        idxBuf.clear();
        PmxNative.nativeCopyIndices(handle, idxBuf);

        indicesCopiedOnce = true;
    }

    private void copyDynamicVertices() {
        posBuf.clear();
        nrmBuf.clear();
        uvBuf.clear();

        PmxNative.nativeCopyPositions(handle, posBuf);
        PmxNative.nativeCopyNormals(handle, nrmBuf);
        PmxNative.nativeCopyUVs(handle, uvBuf);
    }

    private void buildSubmeshInfos() {
        int subCount = PmxNative.nativeGetSubmeshCount(handle);
        submeshes = new SubmeshInfo[subCount];

        for (int s = 0; s < subCount; s++) {
            int begin = PmxNative.nativeGetSubmeshBeginIndex(handle, s);
            int count = PmxNative.nativeGetSubmeshIndexCount(handle, s);
            int matId = PmxNative.nativeGetSubmeshMaterialId(handle, s);
            submeshes[s] = new SubmeshInfo(begin, count, matId);
        }
    }

    private void buildMaterials() {
        int matCount = PmxNative.nativeGetMaterialCount(handle);
        materials = new MaterialInfo[matCount];

        for (int m = 0; m < matCount; m++) {
            int diffuseRGBA = PmxNative.nativeGetMaterialDiffuseRGBA(handle, m);
            float alpha     = PmxNative.nativeGetMaterialAlpha(handle, m);

            final int mm1 = m;
            float[] ambient  = readVec3(dst -> PmxNative.nativeGetMaterialAmbient(handle, mm1, dst));
            float[] specular = readVec3(dst -> PmxNative.nativeGetMaterialSpecular(handle, mm1, dst));

            float specPow = PmxNative.nativeGetMaterialSpecularPower(handle, m);
            boolean bothFace = PmxNative.nativeGetMaterialBothFaceByMaterial(handle, m);

            String mainPath   = resolveTexturePathString(PmxNative.nativeGetMaterialTexturePath(handle, m));
            String toonPath   = resolveTexturePathString(PmxNative.nativeGetMaterialToonTexturePath(handle, m));
            String spherePath = resolveTexturePathString(PmxNative.nativeGetMaterialSphereTexturePath(handle, m));

            final int mm2 = m;
            float[] texMul    = readVec4(dst -> PmxNative.nativeGetMaterialTexMulFactor(handle, mm2, dst));
            float[] texAdd    = readVec4(dst -> PmxNative.nativeGetMaterialTexAddFactor(handle, mm2, dst));
            float[] toonMul   = readVec4(dst -> PmxNative.nativeGetMaterialToonMulFactor(handle, mm2, dst));
            float[] toonAdd   = readVec4(dst -> PmxNative.nativeGetMaterialToonAddFactor(handle, mm2, dst));
            float[] sphereMul = readVec4(dst -> PmxNative.nativeGetMaterialSphereMulFactor(handle, mm2, dst));
            float[] sphereAdd = readVec4(dst -> PmxNative.nativeGetMaterialSphereAddFactor(handle, mm2, dst));

            int sphereMode = PmxNative.nativeGetMaterialSphereMode(handle, m);

            int edgeFlag = PmxNative.nativeGetMaterialEdgeFlag(handle, m);
            float edgeSize = PmxNative.nativeGetMaterialEdgeSize(handle, m);
            final int mm3 = m;
            float[] edgeColor = readVec4(dst -> PmxNative.nativeGetMaterialEdgeColor(handle, mm3, dst));

            boolean groundShadow   = PmxNative.nativeGetMaterialGroundShadow(handle, m);
            boolean shadowCaster   = PmxNative.nativeGetMaterialShadowCaster(handle, m);
            boolean shadowReceiver = PmxNative.nativeGetMaterialShadowReceiver(handle, m);

            materials[m] = new MaterialInfo(
                    diffuseRGBA, alpha,

                    ambient[0], ambient[1], ambient[2],
                    specular[0], specular[1], specular[2],
                    specPow,
                    bothFace,

                    mainPath,
                    texMul, texAdd,

                    toonPath,
                    toonMul, toonAdd,

                    spherePath,
                    sphereMode,
                    sphereMul, sphereAdd,

                    edgeFlag != 0,
                    edgeSize,
                    edgeColor,

                    groundShadow,
                    shadowCaster,
                    shadowReceiver
            );
        }
    }

    private interface VecWriter3 { void write(ByteBuffer dst3f); }
    private interface VecWriter4 { void write(ByteBuffer dst4f); }

    private float[] readVec3(VecWriter3 w) {
        tmp3f.clear();
        w.write(tmp3f);
        tmp3f.rewind();
        return new float[]{ tmp3f.getFloat(), tmp3f.getFloat(), tmp3f.getFloat() };
    }

    private float[] readVec4(VecWriter4 w) {
        tmp4f.clear();
        w.write(tmp4f);
        tmp4f.rewind();
        return new float[]{ tmp4f.getFloat(), tmp4f.getFloat(), tmp4f.getFloat(), tmp4f.getFloat() };
    }

    private String resolveTexturePathString(String texPath) {
        if (texPath == null || texPath.isEmpty()) return null;
        try {
            Path p = Paths.get(texPath);
            if (p.isAbsolute()) return p.normalize().toString();
            if (pmxBaseDir != null) return pmxBaseDir.resolve(p).normalize().toString();
            return p.normalize().toString();
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static String cleanText(String value) {
        if (value == null) return null;
        String trimmed = value.replace("\r", "").trim();
        return trimmed.isEmpty() ? null : trimmed;
    }

    private static Path ensureToonDir() {
        if (cachedToonDir != null) return cachedToonDir;
        Path outDir = Paths.get(System.getProperty("java.io.tmpdir"), "pmx_steve_mod", "toon");
        try {
            Files.createDirectories(outDir);
        } catch (IOException e) {
            LOGGER.warn("[PMX] failed to create toon cache dir", e);
            return null;
        }
        Minecraft mc = Minecraft.getInstance();
        for (int i = 1; i <= 10; i++) {
            String name = String.format("toon%02d.bmp", i);
            Path outFile = outDir.resolve(name);
            if (Files.exists(outFile)) continue;
            ResourceLocation id = ResourceLocation.fromNamespaceAndPath(
                    PmxSteveMod.MOD_ID,
                    "textures/pmx/" + name
            );
            try (InputStream in = mc.getResourceManager().getResource(id).orElseThrow().open()) {
                Files.copy(in, outFile, StandardCopyOption.REPLACE_EXISTING);
            } catch (Throwable t) {
                LOGGER.warn("[PMX] missing builtin toon {}", name, t);
            }
        }
        cachedToonDir = outDir;
        return cachedToonDir;
    }

    public record SubmeshInfo(
            int beginIndex,
            int indexCount,
            int materialId
    ) {}

    public record MaterialInfo(
            int diffuseRGBA,
            float alpha,

            float ambientR, float ambientG, float ambientB,
            float specularR, float specularG, float specularB,
            float specularPower,
            boolean bothFace,

            String mainTexPath,
            float[] texMul, float[] texAdd,

            String toonTexPath,
            float[] toonMul, float[] toonAdd,

            String sphereTexPath,
            int sphereMode,
            float[] sphereMul, float[] sphereAdd,

            boolean edgeFlag,
            float edgeSize,
            float[] edgeColor,

            boolean groundShadow,
            boolean shadowCaster,
            boolean shadowReceiver
    ) {}

    public record ModelInfo(
            String name,
            String nameEn,
            String comment,
            String commentEn
    ) {}
}


