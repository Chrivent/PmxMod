#pragma once

#include "../../Core/Animation/Camera/CameraAnimation.h"

#include <filesystem>

namespace Chrivent {
	class InputManager;
	class Sound;
	struct ViewerInfo;

	class CameraManager {
		bool paused = true;
		bool useMotionCamera = true;
		glm::vec3 freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
		float freeCamYaw = 0.0f;
		float freeCamPitch = 0.0f;
		std::unique_ptr<CameraAnimation> cameraAnim;

		// 자유 카메라 위치와 회전값을 기본 시점으로 되돌린다.
		void ResetFreeCamera();

	public:
		CameraManager();
		~CameraManager();

		uint32_t GetLastFrame() const;
		const std::vector<CameraAnimationKey>& GetAnimationKeys() const;
		bool IsPlaying() const { return !paused; }

		// 모션 패널 모드에 맞춰 자유 카메라와 모션 카메라를 전환한다.
		void SetMotionCameraEnabled(bool enabled);
		// 지정한 프레임으로 재생 시간을 이동한다.
		void SeekFrame(ViewerInfo& viewerInfo, Sound& music, int frame, std::chrono::steady_clock::time_point& saveTime) const;
		// 카메라와 재생 상태를 기본값으로 초기화한다.
		void Reset();
		// 카메라 VMD 파일을 읽어 모션 카메라 애니메이션을 준비한다.
		void LoadCameraAnim(const std::filesystem::path& cameraAnimPath);
		// 일시정지와 사운드 동기화를 반영해 뷰어 애니메이션 시간을 갱신한다.
		void StepTime(ViewerInfo& viewerInfo, Sound& music, std::chrono::steady_clock::time_point& saveTime) const;
		// 재생 상태로 전환한다.
		void Play(Sound& music);
		// 일시정지 상태로 전환한다.
		void Pause(Sound& music);
		// 재생 시간을 처음으로 되돌리고 일시정지한다.
		void Stop(ViewerInfo& viewerInfo, Sound& music, std::chrono::steady_clock::time_point& saveTime);
		// 입력 매니저가 정리한 조작 상태를 카메라와 재생 상태에 반영한다.
		void HandleInput(const InputManager& inputManager, const ViewerInfo& viewerInfo, Sound& music);
		// 현재 카메라 모드에 맞춰 뷰어의 view/projection 행렬을 갱신한다.
		void UpdateCamera(ViewerInfo& viewerInfo) const;
	};
}
