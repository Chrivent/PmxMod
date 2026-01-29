package net.Chivent.pmxSteveMod.viewer;

import net.Chivent.pmxSteveMod.jni.PmxNative;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Path;

final class PmxMusicController {
    private boolean active;
    private boolean useMusicSync;
    private float currentMusicEndFrame;
    private boolean currentMusicLonger;
    private boolean currentMusicSync;
    private Path currentMusicPath;
    private Path currentMusicSourcePath;
    private float lastMusicTime = -1f;
    private final ByteBuffer musicTimes = ByteBuffer.allocateDirect(2 * 4).order(ByteOrder.nativeOrder());

    boolean isActive() { return active; }
    boolean useMusicSync() { return useMusicSync; }
    boolean isMusicLonger() { return currentMusicLonger; }
    boolean isMusicSync() { return currentMusicSync; }
    float currentMusicEndFrame() { return currentMusicEndFrame; }
    Path getCurrentMusicSourcePath() { return currentMusicSourcePath; }

    void reset() {
        active = false;
        useMusicSync = false;
        currentMusicEndFrame = 0f;
        currentMusicLonger = false;
        currentMusicSync = false;
        currentMusicPath = null;
        currentMusicSourcePath = null;
        lastMusicTime = -1f;
    }

    void resetForNewMotion(boolean musicSync) {
        active = false;
        useMusicSync = false;
        currentMusicEndFrame = 0f;
        currentMusicLonger = false;
        currentMusicSync = musicSync;
        currentMusicPath = null;
        currentMusicSourcePath = null;
        lastMusicTime = -1f;
    }

    void stopPlayback(long handle) {
        if (handle != 0L) {
            try { PmxNative.nativeStopMusic(handle); } catch (Throwable ignored) {}
        }
        active = false;
        lastMusicTime = -1f;
    }

    void setVolume(long handle, float volume) {
        if (handle == 0L) return;
        float v = Math.max(0.0f, Math.min(1.0f, volume));
        try {
            PmxNative.nativeSetMusicVolume(handle, v);
        } catch (UnsatisfiedLinkError ignored) {
        }
    }

    void start(long handle, Path safeMusic, Path sourcePath, boolean loop, boolean musicSync, float motionEndFrame) {
        currentMusicPath = safeMusic;
        currentMusicSourcePath = sourcePath;
        currentMusicSync = musicSync;

        if (musicSync) {
            try {
                active = PmxNative.nativePlayMusicLoop(handle, safeMusic.toString(), false);
            } catch (UnsatisfiedLinkError e) {
                active = false;
            }
        } else {
            try {
                active = PmxNative.nativePlayMusicLoop(handle, safeMusic.toString(), loop);
            } catch (UnsatisfiedLinkError e) {
                active = false;
            }
        }

        double lengthSec = 0.0;
        if (active) {
            lengthSec = PmxNative.nativeGetMusicLengthSec(handle);
            try {
                float volume = net.Chivent.pmxSteveMod.client.settings.PmxSoundSettingsStore.get().getMusicVolume();
                PmxNative.nativeSetMusicVolume(handle, volume);
            } catch (UnsatisfiedLinkError ignored) {
            }
        }
        currentMusicEndFrame = lengthSec > 0.0 ? (float) (lengthSec * 30.0) : 0f;
        currentMusicLonger = musicSync
                && currentMusicEndFrame > 0f
                && (motionEndFrame <= 0f || currentMusicEndFrame > motionEndFrame);
        useMusicSync = currentMusicLonger;
        if (active && musicSync && loop && currentMusicLonger) {
            try {
                active = PmxNative.nativePlayMusicLoop(handle, safeMusic.toString(), true);
            } catch (UnsatisfiedLinkError e) {
                active = false;
            }
        }
        lastMusicTime = -1f;
    }

    void onMotionLoop(long handle) {
        if (currentMusicPath == null) return;
        if (currentMusicSync && !currentMusicLonger) {
            try {
                active = PmxNative.nativePlayMusicLoop(handle, currentMusicPath.toString(), false);
            } catch (UnsatisfiedLinkError e) {
                active = false;
            }
            lastMusicTime = -1f;
        } else if (!currentMusicSync) {
            try {
                active = PmxNative.nativePlayMusicLoop(handle, currentMusicPath.toString(), true);
            } catch (UnsatisfiedLinkError e) {
                active = false;
            }
            lastMusicTime = -1f;
        }
    }

    MusicTickResult updateFrame(long handle, float frame, float dt) {
        if (!active) return new MusicTickResult(frame, dt);
        musicTimes.clear();
        boolean ok;
        try {
            ok = PmxNative.nativeGetMusicTimes(handle, musicTimes);
        } catch (UnsatisfiedLinkError e) {
            ok = false;
        }
        musicTimes.rewind();
        if (ok) {
            float musicDt = musicTimes.getFloat();
            float t = musicTimes.getFloat();
            boolean advanced = lastMusicTime < 0f || t > lastMusicTime + 1.0e-4f;
            if (advanced) {
                lastMusicTime = t;
            }
            if (useMusicSync) {
                frame = t * 30.0f;
                dt = musicDt;
            }
        } else {
            active = false;
            lastMusicTime = -1f;
        }
        return new MusicTickResult(frame, dt);
    }

    record MusicTickResult(float frame, float dt) {}
}
