package net.Chivent.pmxSteveMod.viewer;

import com.mojang.logging.LogUtils;
import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.jni.PmxNative;
import net.minecraft.client.Minecraft;
import net.minecraft.resources.ResourceLocation;
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
    private boolean hasMotion = false;
    private boolean forceBlendNext = false;
    private boolean musicActive = false;
    private boolean cameraActive = false;
    private Path currentMotionPath;
    private float currentMotionEndFrame = 0f;
    private boolean currentMotionLoop = false;
    private boolean motionEnded = false;
    private boolean useMusicSync = false;
    private float currentMusicEndFrame = 0f;
    private boolean currentMusicLonger = false;
    private boolean currentMusicSync = false;
    private Path currentMusicPath;
    private float camInterestX;
    private float camInterestY;
    private float camInterestZ;
    private float camRotX;
    private float camRotY;
    private float camRotZ;
    private float camDistance;
    private float camFov;

    private float frame = 0f;
    private long lastNanos = -1;
    private float lastMusicTime = -1f;

    private ByteBuffer idxBuf;
    private ByteBuffer posBuf;
    private ByteBuffer nrmBuf;
    private ByteBuffer uvBuf;

    private Path pmxBaseDir = null;

    private SubmeshInfo[] submeshes;
    private MaterialInfo[] materials;

    private final ByteBuffer tmp3f = ByteBuffer.allocateDirect(3 * 4).order(ByteOrder.nativeOrder());
    private final ByteBuffer tmp4f = ByteBuffer.allocateDirect(4 * 4).order(ByteOrder.nativeOrder());
    private final ByteBuffer musicTimes = ByteBuffer.allocateDirect(2 * 4).order(ByteOrder.nativeOrder());
    private final ByteBuffer cameraState = ByteBuffer.allocateDirect(8 * 4).order(ByteOrder.nativeOrder());

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
            hasMotion = false;
            musicActive = false;
            cameraActive = false;
            currentMotionEndFrame = 0f;
            currentMotionLoop = false;
            motionEnded = false;
            useMusicSync = false;
            currentMusicEndFrame = 0f;
            currentMusicLonger = false;
            currentMusicSync = false;
            currentMusicPath = null;
        } catch (Throwable t) {
            LOGGER.error("[PMX] init error", t);
            ready = false;
        }
    }

    public void shutdown() {
        if (handle != 0L) {
            try { PmxNative.nativeStopMusic(handle); } catch (Throwable ignored) {}
            try { PmxNative.nativeDestroy(handle); } catch (Throwable ignored) {}
            handle = 0L;
        }
        ready = false;
        currentPmxPath = null;
        hasMotion = false;
        forceBlendNext = false;
        musicActive = false;
        cameraActive = false;
        currentMotionPath = null;
        currentMotionEndFrame = 0f;
        currentMotionLoop = false;
        motionEnded = false;
        useMusicSync = false;
        currentMusicEndFrame = 0f;
        currentMusicLonger = false;
        currentMusicSync = false;
        currentMusicPath = null;

        indicesCopiedOnce = false;
        frame = 0f;
        lastNanos = -1;
    }

    public void tickRender() {
        if (!ready || handle == 0L) return;

        long now = System.nanoTime();
        if (lastNanos < 0) lastNanos = now;
        float dt = (now - lastNanos) / 1_000_000_000.0f;
        lastNanos = now;
        frame += dt * 30.0f;

        if (musicActive) {
            musicTimes.clear();
            boolean ok = false;
            try {
                ok = PmxNative.nativeGetMusicTimes(handle, musicTimes);
            } catch (UnsatisfiedLinkError e) {
                musicActive = false;
            }
            musicTimes.rewind();
            if (ok) {
                float musicDt = musicTimes.getFloat();
                float t = musicTimes.getFloat();
                boolean advanced = lastMusicTime < 0f || t > lastMusicTime + 1.0e-4f;
                if (advanced) {
                    lastMusicTime = t;
                }
                if (musicActive && useMusicSync) {
                    frame = t * 30.0f;
                    dt = musicDt;
                }
            } else {
                musicActive = false;
                lastMusicTime = -1f;
            }
        }

        float evalFrame = frame;
        if (useMusicSync && currentMusicLonger && currentMotionEndFrame > 0.0f && frame >= currentMotionEndFrame) {
            evalFrame = currentMotionEndFrame;
            dt = 0.0f;
        }

        float endFrame = currentMotionEndFrame;
        if (currentMusicLonger && currentMusicEndFrame > 0.0f) {
            endFrame = currentMusicEndFrame;
        }
        if (hasMotion && endFrame > 0.0f && frame >= endFrame) {
            if (currentMotionLoop) {
                frame = frame % endFrame;
                lastNanos = -1;
                if (currentMusicSync && !currentMusicLonger && currentMusicPath != null) {
                    try {
                        musicActive = PmxNative.nativePlayMusicLoop(handle, currentMusicPath.toString(), false);
                    } catch (UnsatisfiedLinkError e) {
                        musicActive = false;
                    }
                    lastMusicTime = -1f;
                }
            } else if (!motionEnded) {
                motionEnded = true;
                forceBlendNext = true;
                currentMotionPath = null;
                currentMotionEndFrame = 0f;
            }
        }

        if (musicActive
                && currentMusicSync
                && !currentMusicLonger
                && currentMusicEndFrame > 0.0f
                && frame >= currentMusicEndFrame) {
            try { PmxNative.nativeStopMusic(handle); } catch (Throwable ignored) {}
            musicActive = false;
            lastMusicTime = -1f;
        }

        updateCameraState(evalFrame);
        PmxNative.nativeUpdate(handle, evalFrame, dt);
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
            currentMusicPath = null;
            currentMusicEndFrame = 0f;
            currentMusicLonger = false;
            currentMusicSync = musicSync;
            useMusicSync = false;

            if (cameraPath != null && Files.exists(cameraPath)) {
                Path safeCamera = toSafePath(cameraPath, "camera_cache");
                try {
                    cameraActive = PmxNative.nativeLoadCameraVmd(handle, safeCamera.toString());
                } catch (UnsatisfiedLinkError e) {
                    cameraActive = false;
                }
            } else {
                try { PmxNative.nativeClearCamera(handle); } catch (Throwable ignored) {}
                cameraActive = false;
            }

            Path safePath = toSafePath(vmdPath, "motion_cache");
            float blendSeconds = (hasMotion || forceBlendNext) ? 0.3f : 0.0f;
            forceBlendNext = false;
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
            frame = 0f;
            lastNanos = -1;
            if (!hasMotion) {
                PmxNative.nativeSyncPhysics(handle, 0.0f);
            }
            PmxNative.nativeUpdate(handle, 0.0f, 0.0f);
            hasMotion = true;
            try {
                currentMotionPath = vmdPath.toAbsolutePath().normalize();
            } catch (Exception ignored) {
                currentMotionPath = vmdPath;
            }
            currentMotionEndFrame = PmxNative.nativeGetMotionMaxFrame(handle);
            currentMotionLoop = loop;
            motionEnded = false;

            if (musicPath != null && Files.exists(musicPath)) {
                Path safeMusic = toSafePath(musicPath, "music_cache");
                currentMusicPath = safeMusic;
                if (musicSync) {
                    try {
                        musicActive = PmxNative.nativePlayMusicLoop(handle, safeMusic.toString(), false);
                    } catch (UnsatisfiedLinkError e) {
                        musicActive = false;
                    }
                } else {
                    try {
                        musicActive = PmxNative.nativePlayMusicLoop(handle, safeMusic.toString(), loop);
                    } catch (UnsatisfiedLinkError e) {
                        musicActive = false;
                    }
                }
                double lengthSec = 0.0;
                if (musicActive) {
                    lengthSec = PmxNative.nativeGetMusicLengthSec(handle);
                }
                currentMusicEndFrame = lengthSec > 0.0 ? (float) (lengthSec * 30.0) : 0f;
                boolean musicLonger = musicSync
                        && currentMusicEndFrame > 0f
                        && (currentMotionEndFrame <= 0f || currentMusicEndFrame > currentMotionEndFrame);
                currentMusicLonger = musicLonger;
                useMusicSync = musicLonger;
                if (musicActive && musicSync && loop && musicLonger) {
                    try {
                        musicActive = PmxNative.nativePlayMusicLoop(handle, safeMusic.toString(), true);
                    } catch (UnsatisfiedLinkError e) {
                        musicActive = false;
                    }
                }
            } else {
                try { PmxNative.nativeStopMusic(handle); } catch (Throwable ignored) {}
                musicActive = false;
            }
            lastMusicTime = -1f;
        } catch (Throwable t) {
            LOGGER.warn("[PMX] failed to play motion {}", vmdPath, t);
        }
    }

    public boolean hasCamera() { return cameraActive; }
    public Path getCurrentMotionPath() { return currentMotionPath; }
    public boolean consumeMotionEnded() {
        if (!motionEnded) return false;
        motionEnded = false;
        return true;
    }

    public void markBlendNext() {
        forceBlendNext = true;
    }
    public float camInterestX() { return camInterestX; }
    public float camInterestY() { return camInterestY; }
    public float camInterestZ() { return camInterestZ; }
    public float camRotX() { return camRotX; }
    public float camRotY() { return camRotY; }
    public float camRotZ() { return camRotZ; }
    public float camDistance() { return camDistance; }
    public float camFov() { return camFov; }

    public void stopMusic() {
        if (handle != 0L) {
            try { PmxNative.nativeStopMusic(handle); } catch (Throwable ignored) {}
        }
        musicActive = false;
    }

    public void resetToDefaultPose() {
        Path pmxPath = currentPmxPath;
        if (pmxPath == null) return;
        if (!ready || handle == 0L) {
            init(pmxPath);
            return;
        }
        try {
            PmxNative.nativeStopMusic(handle);
        } catch (Throwable ignored) {
        }
        try {
            PmxNative.nativeClearCamera(handle);
        } catch (Throwable ignored) {
        }
        musicActive = false;
        cameraActive = false;
        boolean started = false;
        try {
            started = PmxNative.nativeStartDefaultPoseBlend(handle, 0.3f);
        } catch (UnsatisfiedLinkError e) {
            LOGGER.warn("[PMX] nativeStartDefaultPoseBlend missing (rebuild native DLL). Falling back to reset.");
        }
        if (!started) {
            shutdown();
            init(pmxPath);
            forceBlendNext = true;
            return;
        }
        hasMotion = false;
        currentMotionPath = null;
        currentMotionEndFrame = 0f;
        currentMotionLoop = false;
        motionEnded = false;
        useMusicSync = false;
        currentMusicEndFrame = 0f;
        currentMusicLonger = false;
        currentMusicSync = false;
        currentMusicPath = null;
        forceBlendNext = true;
        frame = 0f;
        lastNanos = -1;
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

    private void updateCameraState(float frame) {
        if (!cameraActive) return;
        cameraState.clear();
        boolean ok;
        try {
            ok = PmxNative.nativeGetCameraState(handle, frame, cameraState);
        } catch (UnsatisfiedLinkError e) {
            ok = false;
        }
        if (!ok) {
            cameraActive = false;
            return;
        }
        cameraState.rewind();
        camInterestX = cameraState.getFloat();
        camInterestY = cameraState.getFloat();
        camInterestZ = cameraState.getFloat();
        camRotX = cameraState.getFloat();
        camRotY = cameraState.getFloat();
        camRotZ = cameraState.getFloat();
        camDistance = cameraState.getFloat();
        camFov = cameraState.getFloat();
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
