package net.Chivent.pmxSteveMod.viewer.controllers;

import java.nio.file.Path;

public final class PmxPlaybackController {
    private boolean hasMotion;
    private boolean forceBlendNext;
    private Path currentMotionPath;
    private float currentMotionEndFrame;
    private boolean currentMotionLoop;
    private boolean motionEnded;
    private float frame;
    private long lastNanos = -1;
    private float physicsBlendHold;
    private float physicsBlendDuration;

    public record Tick(float frame, float dt) {}

    public Tick tickBase() {
        long now = System.nanoTime();
        if (lastNanos < 0) lastNanos = now;
        float dt = (now - lastNanos) / 1_000_000_000.0f;
        lastNanos = now;
        frame += dt * 30.0f;
        return new Tick(frame, dt);
    }

    public void setFrame(float frame) {
        this.frame = frame;
    }

    public float frame() { return frame; }
    public void resetFrameClock() {
        frame = 0f;
        lastNanos = -1;
    }

    public float computePhysicsDt(float dt) {
        float physicsDt = dt;
        if (physicsBlendHold > 0.0f && physicsBlendDuration > 0.0f) {
            float factor = 1.0f - (physicsBlendHold / physicsBlendDuration);
            factor = Math.max(0.0f, Math.min(1.0f, factor));
            physicsDt = dt * factor;
            physicsBlendHold = Math.max(0.0f, physicsBlendHold - dt);
        }
        return physicsDt;
    }

    public void resetAll() {
        hasMotion = false;
        forceBlendNext = false;
        currentMotionPath = null;
        currentMotionEndFrame = 0f;
        currentMotionLoop = false;
        motionEnded = false;
        frame = 0f;
        lastNanos = -1;
        physicsBlendHold = 0f;
        physicsBlendDuration = 0f;
    }

    public void resetPhysicsBlend(float hold, float duration) {
        physicsBlendHold = hold;
        physicsBlendDuration = duration;
    }

    public boolean hasMotion() { return hasMotion; }
    public Path currentMotionPath() { return currentMotionPath; }
    public float currentMotionEndFrame() { return currentMotionEndFrame; }

    public void setMotionEndFrame(float endFrame) {
        currentMotionEndFrame = endFrame;
    }

    public boolean shouldBlendNext() {
        return hasMotion || forceBlendNext;
    }

    public void clearForceBlend() {
        forceBlendNext = false;
    }

    public void markBlendNext() {
        forceBlendNext = true;
    }

    public void onMotionStarted(Path motionPath, boolean loop, float blendSeconds) {
        hasMotion = true;
        currentMotionPath = motionPath;
        currentMotionEndFrame = (float) 0.0;
        currentMotionLoop = loop;
        motionEnded = false;
        physicsBlendHold = blendSeconds;
        physicsBlendDuration = blendSeconds;
    }

    public void onMotionEnded() {
        motionEnded = true;
        forceBlendNext = true;
        currentMotionPath = null;
        currentMotionEndFrame = 0f;
    }

    public boolean consumeMotionEnded() {
        if (!motionEnded) return false;
        motionEnded = false;
        return true;
    }

    public void handleEndFrame(float endFrame, Runnable onLoop) {
        if (!hasMotion || endFrame <= 0.0f || frame < endFrame) return;
        if (currentMotionLoop) {
            frame = frame % endFrame;
            lastNanos = -1;
            if (onLoop != null) onLoop.run();
        } else if (!motionEnded) {
            onMotionEnded();
        }
    }
}
