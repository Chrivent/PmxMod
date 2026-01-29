package net.Chivent.pmxSteveMod.viewer.controllers;

import net.Chivent.pmxSteveMod.jni.PmxNative;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Path;

public final class PmxCameraController {
    private boolean active;
    private Path currentCameraPath;
    private float camInterestX;
    private float camInterestY;
    private float camInterestZ;
    private float camRotX;
    private float camRotY;
    private float camRotZ;
    private float camDistance;
    private float camFov;
    private final ByteBuffer cameraState = ByteBuffer.allocateDirect(8 * 4).order(ByteOrder.nativeOrder());

    public boolean isActive() { return active; }
    public Path getCurrentCameraPath() { return currentCameraPath; }
    public float camInterestX() { return camInterestX; }
    public float camInterestY() { return camInterestY; }
    public float camInterestZ() { return camInterestZ; }
    public float camRotX() { return camRotX; }
    public float camRotY() { return camRotY; }
    public float camRotZ() { return camRotZ; }
    public float camDistance() { return camDistance; }
    public float camFov() { return camFov; }

    public void reset() {
        active = false;
        currentCameraPath = null;
    }

    public void clear(long handle) {
        try { PmxNative.nativeClearCamera(handle); } catch (Throwable ignored) {}
        active = false;
        currentCameraPath = null;
    }

    public void load(long handle, Path sourcePath, Path safePath) {
        if (safePath == null) {
            clear(handle);
            return;
        }
        try {
            active = PmxNative.nativeLoadCameraVmd(handle, safePath.toString());
        } catch (UnsatisfiedLinkError e) {
            active = false;
        }
        currentCameraPath = sourcePath;
    }

    public void update(long handle, float frame) {
        if (!active) return;
        cameraState.clear();
        boolean ok;
        try {
            ok = PmxNative.nativeGetCameraState(handle, frame, cameraState);
        } catch (UnsatisfiedLinkError e) {
            ok = false;
        }
        if (!ok) {
            active = false;
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
}
