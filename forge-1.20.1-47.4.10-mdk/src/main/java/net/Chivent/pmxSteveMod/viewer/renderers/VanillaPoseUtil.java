package net.Chivent.pmxSteveMod.viewer.renderers;

import com.mojang.blaze3d.vertex.PoseStack;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.util.Mth;
import net.minecraft.core.Direction;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.phys.Vec3;

public final class VanillaPoseUtil {
    private VanillaPoseUtil() {}

    public static float getBodyYawWithHeadClamp(AbstractClientPlayer player, float partialTick) {
        if (player == null) return 0.0f;
        float bodyYaw = Mth.rotLerp(partialTick, player.yBodyRotO, player.yBodyRot);
        if (player.isPassenger() && player.getVehicle() instanceof net.minecraft.world.entity.LivingEntity living) {
            float headYaw = Mth.rotLerp(partialTick, player.yHeadRotO, player.yHeadRot);
            float vehicleBodyYaw = Mth.rotLerp(partialTick, living.yBodyRotO, living.yBodyRot);
            float yawDiff = headYaw - vehicleBodyYaw;
            float clamped = Mth.clamp(Mth.wrapDegrees(yawDiff), -85.0f, 85.0f);
            bodyYaw = headYaw - clamped;
            if (clamped * clamped > 2500.0f) {
                bodyYaw += clamped * 0.2f;
            }
        }
        return bodyYaw;
    }

    public static float getClampedHeadYawDiff(AbstractClientPlayer player, float partialTick) {
        if (player == null) return 0.0f;
        float bodyYaw = getBodyYawWithHeadClamp(player, partialTick);
        float headYaw = Mth.rotLerp(partialTick, player.yHeadRotO, player.yHeadRot);
        float yawDiff = headYaw - bodyYaw;
        return Mth.clamp(Mth.wrapDegrees(yawDiff), -85.0f, 85.0f);
    }

    public static float getShakingYawOffset(AbstractClientPlayer player) {
        if (player == null || !player.isFullyFrozen()) return 0.0f;
        return (float) (Math.cos((double) player.tickCount * 3.25D) * Math.PI * 0.4F);
    }

    public static boolean applySleepPose(AbstractClientPlayer player, PoseStack poseStack, float rotationYaw) {
        if (player == null || poseStack == null) return false;
        if (!player.isSleeping()) return false;

        Direction bedDir = player.getBedOrientation();
        if (bedDir != null) {
            float offset = player.getEyeHeight(Pose.STANDING) - 0.1F;
            poseStack.translate((float) bedDir.getStepX() * -offset, 0.0F, (float) bedDir.getStepZ() * -offset);
            poseStack.mulPose(com.mojang.math.Axis.YP.rotationDegrees(sleepDirectionToRotation(bedDir) + 180.0F));
        } else {
            poseStack.mulPose(com.mojang.math.Axis.YP.rotationDegrees(rotationYaw));
        }
        poseStack.mulPose(com.mojang.math.Axis.ZP.rotationDegrees(-90.0F));
        poseStack.mulPose(com.mojang.math.Axis.YP.rotationDegrees(270.0F));
        return true;
    }

    public static void applyBodyTilt(AbstractClientPlayer player, float partialTick, PoseStack poseStack) {
        if (player == null || poseStack == null) return;

        if (player.isAutoSpinAttack()) {
            poseStack.mulPose(com.mojang.math.Axis.XP.rotationDegrees(90.0F + player.getXRot()));
            poseStack.mulPose(com.mojang.math.Axis.YP.rotationDegrees(((float) player.tickCount + partialTick) * -75.0F));
            return;
        }

        float swimAmount = player.getSwimAmount(partialTick);
        if (player.isFallFlying()) {
            float flyTicks = player.getFallFlyingTicks() + partialTick;
            float rotScale = Mth.clamp(flyTicks * flyTicks / 100.0f, 0.0f, 1.0f);
            if (!player.isAutoSpinAttack()) {
                poseStack.mulPose(com.mojang.math.Axis.XP.rotationDegrees(rotScale * (90.0f + player.getXRot())));
            }
            Vec3 view = player.getViewVector(partialTick);
            Vec3 delta = player.getDeltaMovementLerped(partialTick);
            double d0 = delta.horizontalDistanceSqr();
            double d1 = view.horizontalDistanceSqr();
            if (d0 > 0.0D && d1 > 0.0D) {
                double d2 = (delta.x * view.x + delta.z * view.z) / Math.sqrt(d0 * d1);
                double d3 = delta.x * view.z - delta.z * view.x;
                poseStack.mulPose(com.mojang.math.Axis.YP.rotation((float) (Math.signum(d3) * Math.acos(d2))));
            }
            return;
        }

        if (swimAmount > 0.0f) {
            float target = player.isInWater()
                    || player.isInFluidType((fluidType, height) -> player.canSwimInFluidType(fluidType))
                    ? 90.0f + player.getXRot()
                    : 90.0f;
            float rot = Mth.lerp(swimAmount, 0.0f, target);
            poseStack.mulPose(com.mojang.math.Axis.XP.rotationDegrees(rot));
            if (player.isVisuallySwimming()) {
                poseStack.translate(0.0F, -1.0F, 0.3F);
            }
        }
    }

    public static void applyDeathRotation(AbstractClientPlayer player, float partialTick, PoseStack poseStack) {
        if (player == null || poseStack == null) return;
        if (player.deathTime <= 0) return;
        float progress = (((float) player.deathTime) + partialTick - 1.0F) / 20.0F * 1.6F;
        progress = Mth.sqrt(progress);
        if (progress > 1.0F) progress = 1.0F;
        poseStack.mulPose(com.mojang.math.Axis.ZP.rotationDegrees(-progress * 90.0F));
    }

    private static float sleepDirectionToRotation(Direction facing) {
        if (facing == null) return 0.0F;
        return switch (facing) {
            case SOUTH -> 90.0F;
            case NORTH -> 270.0F;
            case EAST -> 180.0F;
            default -> 0.0F;
        };
    }
}
