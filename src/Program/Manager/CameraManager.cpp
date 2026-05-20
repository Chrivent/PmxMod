#include "CameraManager.h"

#include "InputManager.h"
#include "../../Animation/Camera/CameraAnimation.h"
#include "../../Animation/Camera/CameraAnimationBuilder.h"
#include "../Sound.h"
#include "../../Viewer/Viewer.h"

#include <iostream>

namespace Chrivent {
	glm::mat4 Camera::CalcViewMatrix() const {
		glm::mat4 view(1.0f);
		view = glm::translate(view, glm::vec3(0, 0, -distance));
		glm::mat4 rot(1.0f);
		rot = glm::rotate(rot, rotate.y, glm::vec3(0, 1, 0));
		rot = glm::rotate(rot, rotate.z, glm::vec3(0, 0, -1));
		rot = glm::rotate(rot, rotate.x, glm::vec3(1, 0, 0));
		view = rot * view;
		const glm::vec3 eye = glm::vec3(view[3]) + interest;
		const glm::vec3 center = glm::mat3(view) * glm::vec3(0, 0, -1) + eye;
		const glm::vec3 up = glm::mat3(view) * glm::vec3(0, 1, 0);
		return glm::lookAt(eye, center, up);
	}

	void CameraManager::SyncFreeCameraToCurrentView(const Viewer& viewer) {
		const glm::mat4 invView = glm::inverse(viewer.viewMat);
		freeCamPosition = glm::vec3(invView[3]);
		const glm::vec3 forward = -glm::normalize(glm::vec3(invView[2]));
		freeCamYaw = std::atan2(forward.z, forward.x);
		freeCamPitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
	}

	CameraManager::CameraManager() {
		Reset();
	}

	CameraManager::~CameraManager() = default;
	
	void CameraManager::SeekFrame(Viewer& viewer, Sound& music, const int frame, std::chrono::steady_clock::time_point& saveTime) const {
		const float seconds = static_cast<float>(std::max(0, frame)) / 30.0f;
		viewer.elapsed = 0.0f;
		viewer.animTime = seconds;
		music.SeekSeconds(seconds);
		if (!paused)
			music.Resume();
		saveTime = std::chrono::steady_clock::now();
	}

	void CameraManager::Reset() {
		paused = true;
		useMotionCamera = true;
		hasFreeCameraState = false;
		freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
		freeCamYaw = glm::radians(-90.0f);
		freeCamPitch = 0.0f;
		cameraAnim.reset();
	}

	void CameraManager::LoadCameraAnim(const std::filesystem::path& cameraAnimPath) {
		cameraAnim.reset();
		if (cameraAnimPath.empty()) {
			std::cout << "No camera VMD file.\n";
			return;
		}
		VmdParser camVmd;
		if (camVmd.ReadFile(cameraAnimPath.c_str()) && !camVmd.cameras.empty()) {
			auto vmdCamAnim = std::make_unique<CameraAnimation>();
			CameraAnimationInfo cameraAnimationInfo;
			const CameraAnimationBuilder cameraAnimationBuilder(cameraAnimationInfo);
			if (!cameraAnimationBuilder.Add(camVmd))
				std::cout << "Failed to create VMDCameraAnimation.\n";
			vmdCamAnim->SetInfo(std::move(cameraAnimationInfo));
			cameraAnim = std::move(vmdCamAnim);
		}
	}

	int32_t CameraManager::GetLastFrame() const {
		return cameraAnim ? cameraAnim->GetLastFrame() : 0;
	}

	void CameraManager::StepTime(Viewer& viewer, Sound& music, std::chrono::steady_clock::time_point& saveTime) const {
		const auto now = std::chrono::steady_clock::now();
		double elapsedSeconds = std::chrono::duration<double>(now - saveTime).count();
		if (elapsedSeconds > 1.0 / 30.0)
			elapsedSeconds = 1.0 / 30.0;
		saveTime = now;
		if (paused) {
			viewer.elapsed = 0.0f;
			return;
		}
		const float clockDt = static_cast<float>(elapsedSeconds);
		float dt = clockDt;
		float t = viewer.animTime + dt;
		if (music.hasSound) {
			float audioDt = 0.0f;
			float audioTime = 0.0f;
			music.PullTimes(audioDt, audioTime);
			if (audioDt < 0.0f)
				audioDt = 0.0f;
			if (audioTime > viewer.animTime) {
				dt = audioDt;
				t = audioTime;
			} else {
				dt = clockDt;
				t = viewer.animTime + clockDt;
			}
		}
		viewer.elapsed = dt;
		viewer.animTime = t;
	}

	void CameraManager::Play(Sound& music) {
		paused = false;
		music.Resume();
	}

	void CameraManager::Pause(Sound& music) {
		paused = true;
		music.Pause();
	}

	void CameraManager::Stop(Viewer& viewer, Sound& music, std::chrono::steady_clock::time_point& saveTime) {
		paused = true;
		viewer.elapsed = 0.0f;
		viewer.animTime = 0.0f;
		music.SeekSeconds(0.0f);
		music.Pause();
		saveTime = std::chrono::steady_clock::now();
	}

	void CameraManager::HandleInput(const InputManager& inputManager, const Viewer& viewer, Sound& music) {
		const auto& [togglePause, toggleCameraMode,
			moveForward, moveBackward,
			moveLeft, moveRight,
			moveDown, moveUp,
			rotateCamera, mouseDelta] = inputManager.GetState();
		if (togglePause) {
			paused = !paused;
			if (paused)
				music.Pause();
			else
				music.Resume();
		}
		if (toggleCameraMode) {
			if (useMotionCamera && !hasFreeCameraState) {
				SyncFreeCameraToCurrentView(viewer);
				hasFreeCameraState = true;
			}
			useMotionCamera = !useMotionCamera;
		}
		if (useMotionCamera)
			return;
		const float moveSpeed = 100.0f * std::max(viewer.elapsed, 1.0f / 120.0f);
		glm::vec3 forward(
			std::cos(freeCamPitch) * std::cos(freeCamYaw),
			std::sin(freeCamPitch),
			std::cos(freeCamPitch) * std::sin(freeCamYaw)
		);
		forward = glm::normalize(forward);
		const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
		constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);
		if (moveForward)
			freeCamPosition += forward * moveSpeed;
		if (moveBackward)
			freeCamPosition -= forward * moveSpeed;
		if (moveLeft)
			freeCamPosition -= right * moveSpeed;
		if (moveRight)
			freeCamPosition += right * moveSpeed;
		if (moveDown)
			freeCamPosition -= up * moveSpeed;
		if (moveUp)
			freeCamPosition += up * moveSpeed;
		if (rotateCamera) {
			constexpr float mouseSensitivity = 0.0035f;
			freeCamYaw += mouseDelta.x * mouseSensitivity;
			freeCamPitch -= mouseDelta.y * mouseSensitivity;
			freeCamPitch = std::clamp(freeCamPitch, glm::radians(-89.0f), glm::radians(89.0f));
		}
	}

	void CameraManager::UpdateCamera(Viewer& viewer) const {
		if (useMotionCamera && cameraAnim) {
			cameraAnim->Evaluate(viewer.animTime * 30.0f);
			const auto cam = cameraAnim->GetInfo().camera;
			viewer.viewMat = cam.CalcViewMatrix();
			viewer.projMat = glm::perspectiveFovRH(
				cam.fov, static_cast<float>(viewer.screenWidth), static_cast<float>(viewer.screenHeight), 1.0f, 10000.0f
			);
			return;
		}
		glm::vec3 forward(
			std::cos(freeCamPitch) * std::cos(freeCamYaw),
			std::sin(freeCamPitch),
			std::cos(freeCamPitch) * std::sin(freeCamYaw)
		);
		forward = glm::normalize(forward);
		viewer.viewMat = glm::lookAt(freeCamPosition, freeCamPosition + forward, glm::vec3(0, 1, 0));
		viewer.projMat = glm::perspectiveFovRH(
			glm::radians(30.0f), static_cast<float>(viewer.screenWidth), static_cast<float>(viewer.screenHeight), 1.0f, 10000.0f
		);
	}
}
