package net.Chivent.pmxSteveMod.viewer;

import java.nio.file.Path;

final class PmxPlaybackController {
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

    record Tick(float frame, float dt) {}

    Tick tickBase() {
        long now = System.nanoTime();
        if (lastNanos < 0) lastNanos = now;
        float dt = (now - lastNanos) / 1_000_000_000.0f;
        lastNanos = now;
        frame += dt * 30.0f;
        return new Tick(frame, dt);
    }

    void setFrame(float frame) {
        this.frame = frame;
    }

    float frame() { return frame; }
    void resetFrameClock() {
        frame = 0f;
        lastNanos = -1;
    }

    float computePhysicsDt(float dt) {
        float physicsDt = dt;
        if (physicsBlendHold > 0.0f && physicsBlendDuration > 0.0f) {
            float factor = 1.0f - (physicsBlendHold / physicsBlendDuration);
            factor = Math.max(0.0f, Math.min(1.0f, factor));
            physicsDt = dt * factor;
            physicsBlendHold = Math.max(0.0f, physicsBlendHold - dt);
        }
        return physicsDt;
    }

    void resetAll() {
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

    void resetPhysicsBlend(float hold, float duration) {
        physicsBlendHold = hold;
        physicsBlendDuration = duration;
    }

    boolean hasMotion() { return hasMotion; }
    Path currentMotionPath() { return currentMotionPath; }
    float currentMotionEndFrame() { return currentMotionEndFrame; }

    void setMotionEndFrame(float endFrame) {
        currentMotionEndFrame = endFrame;
    }

    boolean shouldBlendNext() {
        return hasMotion || forceBlendNext;
    }

    void clearForceBlend() {
        forceBlendNext = false;
    }

    void markBlendNext() {
        forceBlendNext = true;
    }

    void onMotionStarted(Path motionPath, boolean loop, float blendSeconds) {
        hasMotion = true;
        currentMotionPath = motionPath;
        currentMotionEndFrame = (float) 0.0;
        currentMotionLoop = loop;
        motionEnded = false;
        physicsBlendHold = blendSeconds;
        physicsBlendDuration = blendSeconds;
    }

    void onMotionEnded() {
        motionEnded = true;
        forceBlendNext = true;
        currentMotionPath = null;
        currentMotionEndFrame = 0f;
    }

    boolean consumeMotionEnded() {
        if (!motionEnded) return false;
        motionEnded = false;
        return true;
    }

    void handleEndFrame(float endFrame, Runnable onLoop) {
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
