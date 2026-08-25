#pragma once

#include "Core/Animation/Camera/CameraAnimation.h"

#include <filesystem>
#include <span>

namespace Chrivent {
	class InputManager;
	class Sound;
	class Viewer;

	// 카메라 모션 평가, 재생 위치와 포스트 프로세스 카메라 입력을 관리한다.
	class CameraManager {
		// 애니메이션 진행과 물리 갱신에 필요한 현재 재생 상태를 묶는다.
		struct PlaybackState {
			float elapsed = 0.0f;
			float renderDeltaTime = 0.0f;
			float animationTime = 0.0f;
			bool skipPhysics = false;
		};

		PlaybackState playbackState;
		bool paused = true;
		bool useMotionCamera = true;
		glm::vec3 freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
		float freeCamYaw = 0.0f;
		float freeCamPitch = 0.0f;
		std::unique_ptr<CameraAnimation> cameraAnim;

		// 자유 카메라 위치와 회전값을 기본 시점으로 되돌린다.
		void ResetFreeCamera();
		// 연속 프레임으로 보기 어려운 카메라 위치, 방향 또는 FOV 변화인지 확인한다.
		static bool IsCameraCut(const glm::vec3& currentPosition, const glm::vec3& previousPosition,
			const glm::vec3& currentDirection, const glm::vec3& previousDirection,
			float currentFov, float previousFov);

	public:
		CameraManager();
		~CameraManager();

		bool IsPlaying() const { return !paused; }
		float GetElapsed() const { return playbackState.elapsed; }
		float GetAnimationFrame() const { return playbackState.animationTime * 30.0f; }
		int GetAnimationFrameIndex() const { return static_cast<int>(GetAnimationFrame() + 0.5f); }
		bool IsPhysicsSkipped() const { return playbackState.skipPhysics; }
		void SetPhysicsSkipped(const bool skipped) { playbackState.skipPhysics = skipped; }
		std::span<const CameraAnimationKey> GetAnimationKeys() const;

		// 카메라 애니메이션의 마지막 프레임을 int 범위로 계산한다.
		int CalculateLastFrame() const;
		// 모션 카메라 사용 상태를 전환한다.
		void ApplyMotionCameraState(Viewer& viewer, bool enabled);
		// 지정한 프레임으로 재생 시간을 이동한다.
		void SeekFrame(Viewer& viewer, Sound& music, int frame, std::chrono::steady_clock::time_point& saveTime);
		// 카메라와 재생 상태를 기본값으로 초기화한다.
		void Reset();
		// 카메라 VMD 파일을 읽어 모션 카메라 애니메이션을 준비한다.
		void LoadCameraAnim(const std::filesystem::path& cameraAnimPath);
		// 일시정지와 사운드 동기화를 반영해 애니메이션 시간을 갱신한다.
		void StepTime(Sound& music, std::chrono::steady_clock::time_point& saveTime);
		// 외부 시간이 고정된 실행에서 재생 시간을 지정한 간격만큼 진행한다.
		void StepFixedTime(float seconds);
		// 재생 상태로 전환한다.
		void Play(Sound& music);
		// 일시정지 상태로 전환한다.
		void Pause(Sound& music);
		// 재생 시간을 처음으로 되돌리고 일시정지한다.
		void Stop(Viewer& viewer, Sound& music, std::chrono::steady_clock::time_point& saveTime);
		// 입력 매니저가 정리한 조작 상태를 카메라와 재생 상태에 반영한다.
		void HandleInput(const InputManager& inputManager, Sound& music);
		// 현재 카메라 모드에 맞춰 뷰어의 view/projection 행렬을 갱신한다.
		void UpdateCamera(Viewer& viewer) const;
	};
}
