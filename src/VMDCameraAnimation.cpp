#include "VMDCameraAnimation.h"

namespace saba
{
	namespace
	{
		void SetVMDBezier(VMDBezier& bezier, const int x0, const int x1, const int y0, const int y1) {
			bezier.m_cp1 = glm::vec2(static_cast<float>(x0) / 127.0f, static_cast<float>(y0) / 127.0f);
			bezier.m_cp2 = glm::vec2(static_cast<float>(x1) / 127.0f, static_cast<float>(y1) / 127.0f);
		}
	} // namespace

	VMDCameraController::VMDCameraController()
		: m_startKeyIndex(0) {
	}

	void VMDCameraController::Evaluate(const float t) {
		if (m_keys.empty())
			return;

		const auto boundIt = FindBoundKey(m_keys, static_cast<int32_t>(t), m_startKeyIndex);
		if (boundIt == std::end(m_keys)) {
			const auto &selectKey = m_keys[m_keys.size() - 1];
			m_camera.m_interest = selectKey.m_interest;
			m_camera.m_rotate = selectKey.m_rotate;
			m_camera.m_distance = selectKey.m_distance;
			m_camera.m_fov = selectKey.m_fov;
		} else {
			const auto &selectKey = *boundIt;
			m_camera.m_interest = selectKey.m_interest;
			m_camera.m_rotate = selectKey.m_rotate;
			m_camera.m_distance = selectKey.m_distance;
			m_camera.m_fov = selectKey.m_fov;
			if (boundIt != std::begin(m_keys)) {
				const auto &key = *(boundIt - 1);
				const auto& [m_time
							, m_interest
							, m_rotate
							, m_distance
							, m_fov
							, m_ixBezier
							, m_iyBezier
							, m_izBezier
							, m_rotateBezier
							, m_distanceBezier
							, m_fovBezier]
				= *boundIt;

				if ((m_time - key.m_time) > 1) {
					const auto timeRange = static_cast<float>(m_time - key.m_time);
					const float time = (t - static_cast<float>(key.m_time)) / timeRange;
					const float ix_x = m_ixBezier.FindBezierX(time);
					const float iy_x = m_iyBezier.FindBezierX(time);
					const float iz_x = m_izBezier.FindBezierX(time);
					const float rotate_x = m_rotateBezier.FindBezierX(time);
					const float distance_x = m_distanceBezier.FindBezierX(time);
					const float fov_x = m_fovBezier.FindBezierX(time);

					const float ix_y = m_ixBezier.EvalY(ix_x);
					const float iy_y = m_iyBezier.EvalY(iy_x);
					const float iz_y = m_izBezier.EvalY(iz_x);
					const float rotate_y = m_rotateBezier.EvalY(rotate_x);
					const float distance_y = m_distanceBezier.EvalY(distance_x);
					const float fov_y = m_fovBezier.EvalY(fov_x);

					m_camera.m_interest = glm::mix(key.m_interest, m_interest, glm::vec3(ix_y, iy_y, iz_y));
					m_camera.m_rotate = glm::mix(key.m_rotate, m_rotate, rotate_y);
					m_camera.m_distance = glm::mix(key.m_distance, m_distance, distance_y);
					m_camera.m_fov = glm::mix(key.m_fov, m_fov, fov_y);
				} else {
					/*
					カメラアニメーションでキーが1フレーム間隔で打たれている場合、
					カメラの切り替えと判定し補間を行わないようにする（key0を使用する）
					*/
					m_camera.m_interest = key.m_interest;
					m_camera.m_rotate = key.m_rotate;
					m_camera.m_distance = key.m_distance;
					m_camera.m_fov = key.m_fov;
				}

				m_startKeyIndex = std::distance(m_keys.cbegin(), boundIt);
			}
		}
	}

	void VMDCameraController::AddKey(const VMDCameraAnimationKey &key) {
		m_keys.push_back(key);
	}

	void VMDCameraController::SortKeys() {
		std::ranges::sort(m_keys,
			[](const VMDCameraAnimationKey &a, const VMDCameraAnimationKey &b)
			{ return a.m_time < b.m_time; }
		);
	}

	VMDCameraAnimation::VMDCameraAnimation() {
		Destroy();
	}

	bool VMDCameraAnimation::Create(const VMDFile & vmd) {
		if (!vmd.m_cameras.empty()) {
			m_cameraController = std::make_unique<VMDCameraController>();
			for (const auto &cam: vmd.m_cameras) {
				VMDCameraAnimationKey key{};
				key.m_time = static_cast<int32_t>(cam.m_frame);
				key.m_interest = cam.m_interest * glm::vec3(1, 1, -1);
				key.m_rotate = cam.m_rotate;
				key.m_distance = cam.m_distance;
				key.m_fov = glm::radians(static_cast<float>(cam.m_viewAngle));

				const uint8_t *ip = cam.m_interpolation.data();
				SetVMDBezier(key.m_ixBezier, ip[0], ip[1], ip[2], ip[3]);
				SetVMDBezier(key.m_iyBezier, ip[4], ip[5], ip[6], ip[7]);
				SetVMDBezier(key.m_izBezier, ip[8], ip[9], ip[10], ip[11]);
				SetVMDBezier(key.m_rotateBezier, ip[12], ip[13], ip[14], ip[15]);
				SetVMDBezier(key.m_distanceBezier, ip[16], ip[17], ip[18], ip[19]);
				SetVMDBezier(key.m_fovBezier, ip[20], ip[21], ip[22], ip[23]);

				m_cameraController->AddKey(key);
			}
			m_cameraController->SortKeys();
		} else
			return false;
		return true;
	}

	void VMDCameraAnimation::Destroy()
	{
		m_cameraController.reset();
	}

	void VMDCameraAnimation::Evaluate(const float t)
	{
		m_cameraController->Evaluate(t);
		m_camera = m_cameraController->m_camera;
	}
}

