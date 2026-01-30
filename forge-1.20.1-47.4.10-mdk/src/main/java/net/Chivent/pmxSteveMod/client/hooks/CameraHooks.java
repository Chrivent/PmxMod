package net.Chivent.pmxSteveMod.client.hooks;

import net.Chivent.pmxSteveMod.PmxSteveMod;
import net.Chivent.pmxSteveMod.viewer.PmxInstance;
import net.Chivent.pmxSteveMod.viewer.PmxViewer;
import net.Chivent.pmxSteveMod.viewer.renderers.PmxRenderBase;
import net.minecraft.client.Camera;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.util.Mth;
import net.minecraft.world.phys.Vec3;
import net.minecraftforge.api.distmarker.Dist;
import net.minecraftforge.client.event.ViewportEvent;
import net.minecraftforge.eventbus.api.SubscribeEvent;
import net.minecraftforge.fml.common.Mod;
import org.joml.Matrix3f;
import org.joml.Matrix4f;
import org.joml.Vector3f;
import java.lang.reflect.Method;

@Mod.EventBusSubscriber(modid = PmxSteveMod.MOD_ID, bus = Mod.EventBusSubscriber.Bus.FORGE, value = Dist.CLIENT)
public final class CameraHooks {
    private static Method cameraSetPos;
    private static boolean triedLookup;

    private CameraHooks() {}

    @SubscribeEvent
    public static void onComputeCameraAngles(ViewportEvent.ComputeCameraAngles event) {
        PmxViewer viewer = PmxViewer.get();
        if (!viewer.isPmxVisible()) return;
        PmxInstance instance = viewer.instance();
        if (instance.hasCamera()) {
            Minecraft mc = Minecraft.getInstance();
            AbstractClientPlayer player = mc.player;
            if (player == null) return;

            float partial = (float) event.getPartialTick();
            float scale = PmxRenderBase.MODEL_SCALE;

            float ix = instance.camInterestX() * scale;
            float iy = instance.camInterestY() * scale;
            float iz = instance.camInterestZ() * scale;
            float rx = instance.camRotX();
            float ry = instance.camRotY();
            float rz = instance.camRotZ();
            float dist = instance.camDistance() * scale;

            Matrix4f view = new Matrix4f().translation(0.0f, 0.0f, -dist);
            Matrix4f rot = new Matrix4f().rotateY(ry).rotateZ(rz).rotateX(rx);
            rot.mul(view, view);

            Vector3f eye = new Vector3f(view.m30(), view.m31(), view.m32()).add(ix, iy, iz);
            Vector3f center = new Vector3f(0.0f, 0.0f, -1.0f);
            new Matrix3f(view).transform(center).add(eye);
            Vector3f up = new Vector3f(0.0f, 1.0f, 0.0f);
            new Matrix3f(view).transform(up);

            float playerYaw = player.getViewYRot(partial);
            Matrix3f yawRot = new Matrix3f().rotateY((float) Math.toRadians(-playerYaw));
            yawRot.transform(eye);
            yawRot.transform(center);
            yawRot.transform(up);

            double baseX = Mth.lerp(partial, player.xo, player.getX());
            double baseY = Mth.lerp(partial, player.yo, player.getY());
            double baseZ = Mth.lerp(partial, player.zo, player.getZ());

            Vector3f worldEye = new Vector3f(eye).add((float) baseX, (float) baseY, (float) baseZ);
            Vector3f worldCenter = new Vector3f(center).add((float) baseX, (float) baseY, (float) baseZ);

            Vector3f dir = worldCenter.sub(worldEye, new Vector3f());
            float len = dir.length();
            if (len < 1.0e-6f) return;

            float yaw = (float) Math.toDegrees(Math.atan2(-dir.x, dir.z));
            float pitch = (float) Math.toDegrees(Math.asin(-dir.y / len));
            float roll = (float) Math.toDegrees(-rz);

            event.setYaw(yaw);
            event.setPitch(pitch);
            event.setRoll(roll);

            setCameraPosition(event.getCamera(), worldEye.x, worldEye.y, worldEye.z);
        }
    }

    @SubscribeEvent
    public static void onComputeFov(ViewportEvent.ComputeFov event) {
        PmxViewer viewer = PmxViewer.get();
        if (!viewer.isPmxVisible()) return;
        PmxInstance instance = viewer.instance();
        if (instance.hasCamera()) {
            float fovRad = instance.camFov();
            if (fovRad <= 0.0f) return;
            event.setFOV(Math.toDegrees(fovRad));
        }
    }

    private static void setCameraPosition(Camera camera, double x, double y, double z) {
        Method m = getSetPositionMethod();
        if (m == null) return;
        try {
            if (m.getParameterCount() == 3) {
                m.invoke(camera, x, y, z);
            } else {
                m.invoke(camera, new Vec3(x, y, z));
            }
        } catch (Throwable ignored) {
        }
    }

    private static Method getSetPositionMethod() {
        if (triedLookup) return cameraSetPos;
        triedLookup = true;
        try {
            Method m = Camera.class.getDeclaredMethod("setPosition", double.class, double.class, double.class);
            m.setAccessible(true);
            cameraSetPos = m;
            return m;
        } catch (Throwable ignored) {
        }
        try {
            Method m = Camera.class.getDeclaredMethod("setPosition", Vec3.class);
            m.setAccessible(true);
            cameraSetPos = m;
            return m;
        } catch (Throwable ignored) {
        }
        return null;
    }
}
