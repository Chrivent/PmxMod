#include "Program/Manager/CameraManager.h"

#include "Program/Manager/InputManager.h"
#include "Core/Animation/Camera/CameraAnimation.h"
#include "Core/Animation/Camera/CameraAnimationBuilder.h"
#include "Core/Parser/BinaryReader.h"
#include "Program/Sound.h"
#include "Viewer/Viewer/Viewer.h"

#include <iostream>
#include <limits>

namespace Chrivent {
	void CameraManager::ResetFreeCamera() {
		freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
		freeCamYaw = glm::radians(-90.0f);
		freeCamPitch = 0.0f;
	}

	CameraManager::CameraManager() {
		Reset();
	}

	CameraManager::~CameraManager() = default;

	int CameraManager::CalculateLastFrame() const {
		if (!cameraAnim)
			return 0;
		return std::min(cameraAnim->GetLastFrame(), static_cast<uint32_t>(std::numeric_limits<int>::max()));
	}

	const std::vector<CameraAnimationKey>& CameraManager::ResolveAnimationKeys() const {
		static constexpr std::vector<CameraAnimationKey> emptyKeys;
		return cameraAnim ? cameraAnim->GetKeys() : emptyKeys;
	}

	void CameraManager::ApplyMotionCameraState(Viewer& viewer, const bool enabled) {
		if (useMotionCamera == enabled)
			return;
		useMotionCamera = enabled;
		viewer.ResetPostProcessHistory();
	}

	void CameraManager::SeekFrame(Viewer& viewer, Sound& music, const int frame, std::chrono::steady_clock::time_point& saveTime) const {
		const float seconds = std::max(0, frame) / 30.0f;
		viewer.elapsed = 0.0f;
		viewer.animTime = seconds;
		music.SeekSeconds(seconds);
		if (!paused)
			music.Resume();
		saveTime = std::chrono::steady_clock::now();
		viewer.ResetPostProcessHistory();
	}

	void CameraManager::Reset() {
		paused = true;
		useMotionCamera = true;
		ResetFreeCamera();
		cameraAnim.reset();
	}

	void CameraManager::LoadCameraAnim(const std::filesystem::path& cameraAnimPath) {
		cameraAnim.reset();
		useMotionCamera = true;
		ResetFreeCamera();
		if (cameraAnimPath.empty()) {
			std::cout << "No camera VMD file.\n";
			return;
		}
		VmdParser camVmd;
		const auto parseResult = camVmd.ReadFile(cameraAnimPath);
		if (!parseResult) {
			std::cerr << "Failed to read camera VMD: " << BinaryReader::FormatParseError(parseResult.error()) << '\n';
			return;
		}
		if (!camVmd.GetData().cameras.empty()) {
			auto cameraKeys = CameraAnimationBuilder::Build(camVmd.GetData());
			if (cameraKeys.empty())
				std::cerr << "Failed to create VMDCameraAnimation.\n";
			cameraAnim = std::make_unique<CameraAnimation>(std::move(cameraKeys));
		}
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
		const float clockDt = elapsedSeconds;
		float dt = clockDt;
		float t = viewer.animTime + dt;
		if (music.HasSound()) {
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
		viewer.ResetPostProcessHistory();
	}

	void CameraManager::HandleInput(const InputManager& inputManager, const Viewer& viewer, Sound& music) {
		const auto& [togglePause,
			moveForward, moveBackward,
			moveLeft, moveRight,
			moveDown, moveUp,
			rotateCamera, mouseDelta, wheelDelta] = inputManager.GetState();
		if (togglePause) {
			paused = !paused;
			if (paused)
				music.Pause();
			else
				music.Resume();
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
		if (wheelDelta != 0.0f)
			freeCamPosition += forward * moveSpeed * wheelDelta * 5.0f;
		if (rotateCamera) {
			constexpr float mouseSensitivity = 0.0035f;
			freeCamYaw += mouseDelta.x * mouseSensitivity;
			freeCamPitch -= mouseDelta.y * mouseSensitivity;
			freeCamPitch = std::clamp(freeCamPitch, glm::radians(-89.0f), glm::radians(89.0f));
		}
	}

	void CameraManager::UpdateCamera(Viewer& viewer) const {
		constexpr float nearPlane = 1.0f;
		constexpr float farPlane = 10000.0f;
		float verticalFov = glm::radians(30.0f);
		if (useMotionCamera && cameraAnim) {
			const auto& cam = cameraAnim->Evaluate(viewer.animTime * 30.0f);
			viewer.viewMat = cam.CalcViewMatrix();
			verticalFov = cam.fov;
		} else {
			glm::vec3 forward(
				std::cos(freeCamPitch) * std::cos(freeCamYaw),
				std::sin(freeCamPitch),
				std::cos(freeCamPitch) * std::sin(freeCamYaw)
			);
			forward = glm::normalize(forward);
			viewer.viewMat = glm::lookAt(freeCamPosition, freeCamPosition + forward, glm::vec3(0, 1, 0));
		}
		const float viewportWidth = static_cast<float>(viewer.screenWidth);
		const float viewportHeight = static_cast<float>(viewer.screenHeight);
		viewer.projMat = glm::perspectiveFovRH(
			verticalFov, viewportWidth, viewportHeight, nearPlane, farPlane
		);
		if (viewer.postProcessHistoryResetPending) {
			viewer.previousViewMat = viewer.viewMat;
			viewer.previousProjMat = viewer.projMat;
		}
		const glm::mat4 inverseView = glm::inverse(viewer.viewMat);
		const glm::mat4 previousInverseView = glm::inverse(viewer.previousViewMat);
		const glm::vec3 cameraPosition = glm::vec3(inverseView[3]);
		const glm::vec3 previousCameraPosition = glm::vec3(previousInverseView[3]);
		const glm::vec3 cameraDirection = glm::normalize(-glm::vec3(inverseView[2]));
		const glm::vec3 previousCameraDirection = glm::normalize(-glm::vec3(previousInverseView[2]));
		const glm::vec3 cameraRight = glm::normalize(glm::vec3(inverseView[0]));
		const glm::vec3 cameraUp = glm::normalize(glm::vec3(inverseView[1]));
		viewer.postProcessFrameData = {
			.deltaTime = std::max(viewer.elapsed, 0.0f),
			.nearPlane = nearPlane,
			.farPlane = farPlane,
			.verticalFovRadians = verticalFov,
			.viewportWidth = viewportWidth,
			.viewportHeight = viewportHeight,
			.inverseViewportWidth = viewportWidth > 0.0f ? 1.0f / viewportWidth : 0.0f,
			.inverseViewportHeight = viewportHeight > 0.0f ? 1.0f / viewportHeight : 0.0f,
			.historyReset = viewer.postProcessHistoryResetPending ? 1.0f : 0.0f,
			.cameraWorldPosition = glm::vec4(cameraPosition, 0.0f),
			.previousCameraWorldPosition = glm::vec4(previousCameraPosition, 0.0f),
			.cameraWorldDirection = glm::vec4(cameraDirection, 0.0f),
			.previousCameraWorldDirection = glm::vec4(previousCameraDirection, 0.0f),
			.cameraWorldRight = glm::vec4(cameraRight, 0.0f),
			.cameraWorldUp = glm::vec4(cameraUp, 0.0f)
		};
	}
}
