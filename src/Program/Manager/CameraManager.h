#pragma once

#include <filesystem>
#include <glm/glm.hpp>

namespace Chrivent {
	struct CameraAnimation;
	class InputManager;
	class Sound;
	class Viewer;

	struct Camera {
		glm::vec3 interest = glm::vec3(0, 10, 0);
		glm::vec3 rotate = glm::vec3(0, 0, 0);
		float distance = 50;
		float fov = glm::radians(30.0f);

		// 현재 카메라 파라미터로 뷰 행렬을 계산한다.
		glm::mat4 CalcViewMatrix() const;
	};

	class CameraManager {
		bool paused = true;
		bool useMotionCamera = true;
		bool hasFreeCameraState = false;
		glm::vec3 freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
		float freeCamYaw = 0.0f;
		float freeCamPitch = 0.0f;
		std::unique_ptr<CameraAnimation> cameraAnim;

		// 현재 모션 카메라 시점에서 자유 카메라 위치와 회전값을 동기화한다.
		void SyncFreeCameraToCurrentView(const Viewer& viewer);

	public:
		CameraManager();
		~CameraManager();

		// 지정한 프레임으로 재생 시간을 이동한다.
		void SeekFrame(Viewer& viewer, Sound& music, int frame, std::chrono::steady_clock::time_point& saveTime) const;
		// 카메라와 재생 상태를 기본값으로 초기화한다.
		void Reset();
		// 카메라 VMD 파일을 읽어 모션 카메라 애니메이션을 준비한다.
		void LoadCameraAnim(const std::filesystem::path& cameraAnimPath);
		// 카메라 모션의 마지막 프레임을 반환한다.
		int32_t GetLastFrame() const;
		// 일시정지와 사운드 동기화를 반영해 뷰어 애니메이션 시간을 갱신한다.
		void StepTime(Viewer& viewer, Sound& music, std::chrono::steady_clock::time_point& saveTime) const;
		// 재생 상태로 전환한다.
		void Play(Sound& music);
		// 일시정지 상태로 전환한다.
		void Pause(Sound& music);
		// 재생 시간을 처음으로 되돌리고 일시정지한다.
		void Stop(Viewer& viewer, Sound& music, std::chrono::steady_clock::time_point& saveTime);
		// 입력 매니저가 정리한 조작 상태를 카메라와 재생 상태에 반영한다.
		void HandleInput(const InputManager& inputManager, const Viewer& viewer, Sound& music);
		// 현재 카메라 모드에 맞춰 뷰어의 view/projection 행렬을 갱신한다.
		void UpdateCamera(Viewer& viewer) const;
	};
}
