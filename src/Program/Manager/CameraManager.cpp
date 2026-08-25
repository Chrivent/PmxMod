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

	bool CameraManager::IsCameraCut(
		const glm::vec3& currentPosition, const glm::vec3& previousPosition,
		const glm::vec3& currentDirection, const glm::vec3& previousDirection,
		const float currentFov, const float previousFov) {
		constexpr float positionThreshold = 20.0f;
		constexpr float directionThreshold = 0.9063078f;
		constexpr float fovThreshold = 0.1745329f;
		return glm::distance(currentPosition, previousPosition) > positionThreshold
			|| glm::dot(currentDirection, previousDirection) < directionThreshold
			|| std::abs(currentFov - previousFov) > fovThreshold;
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

	std::span<const CameraAnimationKey> CameraManager::GetAnimationKeys() const {
		return cameraAnim ? cameraAnim->GetKeys() : std::span<const CameraAnimationKey>{};
	}

	void CameraManager::ApplyMotionCameraState(Viewer& viewer, const bool enabled) {
		if (useMotionCamera == enabled)
			return;
		useMotionCamera = enabled;
		viewer.ResetPostProcessHistory();
	}

	void CameraManager::SeekFrame(Viewer& viewer, Sound& music, const int frame,
		std::chrono::steady_clock::time_point& saveTime) {
		PlaybackState& playback = playbackState;
		const float seconds = std::max(0, frame) / 30.0f;
		playback.elapsed = 0.0f;
		playback.renderDeltaTime = 0.0f;
		playback.animationTime = seconds;
		music.SeekSeconds(seconds);
		if (!paused)
			music.Resume();
		saveTime = std::chrono::steady_clock::now();
		viewer.ResetPostProcessHistory();
	}

	void CameraManager::Reset() {
		playbackState = {};
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
			std::cerr << "카메라 VMD를 읽지 못했습니다: "
				<< BinaryReader::FormatParseError(parseResult.error()) << '\n';
			return;
		}
		if (!camVmd.GetData().cameras.empty()) {
			cameraAnim = CameraAnimationBuilder::Build(camVmd.GetData());
		}
	}

	void CameraManager::StepTime(Sound& music, std::chrono::steady_clock::time_point& saveTime) {
		PlaybackState& playback = playbackState;
		const auto now = std::chrono::steady_clock::now();
		const float frameSeconds = std::max(0.0f, std::chrono::duration<float>(now - saveTime).count());
		playback.renderDeltaTime = std::min(frameSeconds, 1.0f / 15.0f);
		float elapsedSeconds = frameSeconds;
		if (elapsedSeconds > 1.0f / 30.0f)
			elapsedSeconds = 1.0f / 30.0f;
		saveTime = now;
		if (paused) {
			playback.elapsed = 0.0f;
			return;
		}
		const float clockDt = elapsedSeconds;
		float dt = clockDt;
		float t = playback.animationTime + dt;
		if (music.HasSound()) {
			float audioDt = 0.0f;
			float audioTime = 0.0f;
			music.PullTimes(audioDt, audioTime);
			if (audioDt < 0.0f)
				audioDt = 0.0f;
			if (audioTime > playback.animationTime) {
				dt = audioDt;
				t = audioTime;
			} else {
				dt = clockDt;
				t = playback.animationTime + clockDt;
			}
		}
		playback.elapsed = dt;
		playback.animationTime = t;
	}

	void CameraManager::StepFixedTime(const float seconds) {
		playbackState.elapsed = seconds;
		playbackState.renderDeltaTime = seconds;
		playbackState.animationTime += seconds;
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
		PlaybackState& playback = playbackState;
		paused = true;
		playback.elapsed = 0.0f;
		playback.renderDeltaTime = 0.0f;
		playback.animationTime = 0.0f;
		playback.skipPhysics = false;
		music.SeekSeconds(0.0f);
		music.Pause();
		saveTime = std::chrono::steady_clock::now();
		viewer.ResetPostProcessHistory();
	}

	void CameraManager::HandleInput(const InputManager& inputManager, Sound& music) {
		const PlaybackState& playback = playbackState;
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
		const float moveSpeed = 100.0f * std::max(playback.elapsed, 1.0f / 120.0f);
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
		SceneRenderState& scene = viewer.GetSceneRenderState();
		const PlaybackState& playback = playbackState;
		constexpr float nearPlane = 1.0f;
		constexpr float farPlane = 10000.0f;
		float verticalFov = glm::radians(30.0f);
		if (useMotionCamera && cameraAnim) {
			const auto cam = cameraAnim->Evaluate(playback.animationTime * 30.0f);
			scene.viewMatrix = cam.CalculateViewMatrix();
			verticalFov = cam.fov;
		} else {
			glm::vec3 forward(
				std::cos(freeCamPitch) * std::cos(freeCamYaw),
				std::sin(freeCamPitch),
				std::cos(freeCamPitch) * std::sin(freeCamYaw)
			);
			forward = glm::normalize(forward);
			scene.viewMatrix = glm::lookAt(freeCamPosition, freeCamPosition + forward, glm::vec3(0, 1, 0));
		}
		const float viewportWidth = static_cast<float>(viewer.GetScreenWidth());
		const float viewportHeight = static_cast<float>(viewer.GetScreenHeight());
		scene.projectionMatrix = glm::perspectiveFovRH(
			verticalFov, viewportWidth, viewportHeight, nearPlane, farPlane
		);
		const glm::mat4 inverseView = glm::inverse(scene.viewMatrix);
		const auto cameraPosition = glm::vec3(inverseView[3]);
		const glm::vec3 cameraDirection = glm::normalize(-glm::vec3(inverseView[2]));
		if (useMotionCamera && cameraAnim && !viewer.IsPostProcessHistoryResetPending()) {
			const glm::mat4 previousInverseView = glm::inverse(viewer.GetPreviousViewMatrix());
			const auto previousCameraPosition = glm::vec3(previousInverseView[3]);
			const glm::vec3 previousCameraDirection = glm::normalize(-glm::vec3(previousInverseView[2]));
			if (IsCameraCut(cameraPosition, previousCameraPosition, cameraDirection, previousCameraDirection,
				verticalFov, viewer.GetPostProcessFrameData().verticalFovRadians))
				viewer.ResetPostProcessHistory();
		}
		const bool historyResetPending = viewer.IsPostProcessHistoryResetPending();
		const glm::mat4& previousViewMatrix = historyResetPending ? scene.viewMatrix : viewer.GetPreviousViewMatrix();
		const glm::mat4 previousInverseView = glm::inverse(previousViewMatrix);
		const auto previousCameraPosition = glm::vec3(previousInverseView[3]);
		const glm::vec3 previousCameraDirection = glm::normalize(-glm::vec3(previousInverseView[2]));
		const glm::vec3 cameraRight = glm::normalize(glm::vec3(inverseView[0]));
		const glm::vec3 cameraUp = glm::normalize(glm::vec3(inverseView[1]));
		viewer.UpdatePostProcessFrameData({
			.deltaTime = playback.renderDeltaTime,
			.nearPlane = nearPlane,
			.farPlane = farPlane,
			.verticalFovRadians = verticalFov,
			.viewportWidth = viewportWidth,
			.viewportHeight = viewportHeight,
			.inverseViewportWidth = viewportWidth > 0.0f ? 1.0f / viewportWidth : 0.0f,
			.inverseViewportHeight = viewportHeight > 0.0f ? 1.0f / viewportHeight : 0.0f,
			.historyReset = historyResetPending ? 1.0f : 0.0f,
			.cameraWorldPosition = glm::vec4(cameraPosition, 0.0f),
			.previousCameraWorldPosition = glm::vec4(previousCameraPosition, 0.0f),
			.cameraWorldDirection = glm::vec4(cameraDirection, 0.0f),
			.previousCameraWorldDirection = glm::vec4(previousCameraDirection, 0.0f),
			.cameraWorldRight = glm::vec4(cameraRight, 0.0f),
			.cameraWorldUp = glm::vec4(cameraUp, 0.0f)
		});
	}
}
