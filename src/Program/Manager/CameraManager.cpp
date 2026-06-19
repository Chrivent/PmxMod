#include "CameraManager.h"

#include "InputManager.h"
#include "../../Animation/Camera/CameraAnimation.h"
#include "../../Animation/Camera/CameraAnimationBuilder.h"
#include "../Sound.h"
#include "../../Viewer/Viewer.h"

#include <iostream>

namespace Chrivent {
	void CameraManager::SyncFreeCameraToCurrentView(const ViewerInfo& viewerInfo) {
		const glm::mat4 invView = glm::inverse(viewerInfo.viewMat);
		freeCamPosition = glm::vec3(invView[3]);
		const glm::vec3 forward = -glm::normalize(glm::vec3(invView[2]));
		freeCamYaw = std::atan2(forward.z, forward.x);
		freeCamPitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
	}

	CameraManager::CameraManager() {
		Reset();
	}

	CameraManager::~CameraManager() = default;

	uint32_t CameraManager::GetLastFrame() const {
		return cameraAnim ? cameraAnim->GetLastFrame() : 0;
	}

	void CameraManager::SeekFrame(ViewerInfo& viewerInfo, Sound& music, const int frame, std::chrono::steady_clock::time_point& saveTime) const {
		const float seconds = std::max(0, frame) / 30.0f;
		viewerInfo.elapsed = 0.0f;
		viewerInfo.animTime = seconds;
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
		if (camVmd.ReadFile(cameraAnimPath.c_str()) && !camVmd.GetData().cameras.empty()) {
			auto cameraKeys = CameraAnimationBuilder::Build(camVmd.GetData());
			if (cameraKeys.empty())
				std::cerr << "Failed to create VMDCameraAnimation.\n";
			cameraAnim = std::make_unique<CameraAnimation>(std::move(cameraKeys));
		}
	}

	void CameraManager::StepTime(ViewerInfo& viewerInfo, Sound& music, std::chrono::steady_clock::time_point& saveTime) const {
		const auto now = std::chrono::steady_clock::now();
		double elapsedSeconds = std::chrono::duration<double>(now - saveTime).count();
		if (elapsedSeconds > 1.0 / 30.0)
			elapsedSeconds = 1.0 / 30.0;
		saveTime = now;
		if (paused) {
			viewerInfo.elapsed = 0.0f;
			return;
		}
		const float clockDt = elapsedSeconds;
		float dt = clockDt;
		float t = viewerInfo.animTime + dt;
		if (music.HasSound()) {
			float audioDt = 0.0f;
			float audioTime = 0.0f;
			music.PullTimes(audioDt, audioTime);
			if (audioDt < 0.0f)
				audioDt = 0.0f;
			if (audioTime > viewerInfo.animTime) {
				dt = audioDt;
				t = audioTime;
			} else {
				dt = clockDt;
				t = viewerInfo.animTime + clockDt;
			}
		}
		viewerInfo.elapsed = dt;
		viewerInfo.animTime = t;
	}

	void CameraManager::Play(Sound& music) {
		paused = false;
		music.Resume();
	}

	void CameraManager::Pause(Sound& music) {
		paused = true;
		music.Pause();
	}

	void CameraManager::Stop(ViewerInfo& viewerInfo, Sound& music, std::chrono::steady_clock::time_point& saveTime) {
		paused = true;
		viewerInfo.elapsed = 0.0f;
		viewerInfo.animTime = 0.0f;
		music.SeekSeconds(0.0f);
		music.Pause();
		saveTime = std::chrono::steady_clock::now();
	}

	void CameraManager::HandleInput(const InputManager& inputManager, const ViewerInfo& viewerInfo, Sound& music) {
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
				SyncFreeCameraToCurrentView(viewerInfo);
				hasFreeCameraState = true;
			}
			useMotionCamera = !useMotionCamera;
		}
		if (useMotionCamera)
			return;
		const float moveSpeed = 100.0f * std::max(viewerInfo.elapsed, 1.0f / 120.0f);
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

	void CameraManager::UpdateCamera(ViewerInfo& viewerInfo) const {
		if (useMotionCamera && cameraAnim) {
			const auto& cam = cameraAnim->Evaluate(viewerInfo.animTime * 30.0f);
			viewerInfo.viewMat = cam.CalcViewMatrix();
			viewerInfo.projMat = glm::perspectiveFovRH(
				cam.fov, static_cast<float>(viewerInfo.screenWidth), static_cast<float>(viewerInfo.screenHeight), 1.0f, 10000.0f
			);
			return;
		}
		glm::vec3 forward(
			std::cos(freeCamPitch) * std::cos(freeCamYaw),
			std::sin(freeCamPitch),
			std::cos(freeCamPitch) * std::sin(freeCamYaw)
		);
		forward = glm::normalize(forward);
		viewerInfo.viewMat = glm::lookAt(freeCamPosition, freeCamPosition + forward, glm::vec3(0, 1, 0));
		viewerInfo.projMat = glm::perspectiveFovRH(
			glm::radians(30.0f), static_cast<float>(viewerInfo.screenWidth), static_cast<float>(viewerInfo.screenHeight), 1.0f, 10000.0f
		);
	}
}
