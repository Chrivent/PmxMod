#include "CameraAnimationBuilder.h"

namespace Chrivent {
	CameraAnimationKey CameraAnimationBuilder::CreateCameraKey(const auto& camera) {
		CameraAnimationKey key{};
		key.time = static_cast<int32_t>(camera.frame);
		key.interest = camera.interest * glm::vec3(1, 1, -1);
		key.rotate = camera.rotate;
		key.distance = camera.distance;
		key.fov = glm::radians(static_cast<float>(camera.viewAngle));
		key.ixBezier.Assign(
			camera.interpolation[0], camera.interpolation[1],
			camera.interpolation[2], camera.interpolation[3]);
		key.iyBezier.Assign(
			camera.interpolation[4], camera.interpolation[5],
			camera.interpolation[6], camera.interpolation[7]);
		key.izBezier.Assign(
			camera.interpolation[8], camera.interpolation[9],
			camera.interpolation[10], camera.interpolation[11]);
		key.rotateBezier.Assign(
			camera.interpolation[12], camera.interpolation[13],
			camera.interpolation[14], camera.interpolation[15]);
		key.distanceBezier.Assign(
			camera.interpolation[16], camera.interpolation[17],
			camera.interpolation[18], camera.interpolation[19]);
		key.fovBezier.Assign(
			camera.interpolation[20], camera.interpolation[21],
			camera.interpolation[22], camera.interpolation[23]);
		return key;
	}

	bool CameraAnimationBuilder::Add(const VmdParser& vmd) const {
		if (vmd.cameras.empty())
			return false;
		animation.keys.clear();
		for (const auto& camera : vmd.cameras)
			animation.keys.push_back(CreateCameraKey(camera));
		std::ranges::sort(animation.keys, {}, &CameraAnimationKey::time);
		return true;
	}
}
