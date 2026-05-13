#pragma once

#include <memory>
#include <filesystem>
#include <glm/glm.hpp>

class CameraAnimation;
class Sound;
class Viewer;

class Controller {
	bool paused = false;
	bool prevSpaceDown = false;
	bool useMotionCamera = true;
	bool hasFreeCameraState = false;
	bool prevRDown = false;
	bool prevRightMouseDown = false;
	double prevCursorX = 0.0;
	double prevCursorY = 0.0;
	glm::vec3 freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
	float freeCamYaw = 0.0f;
	float freeCamPitch = 0.0f;
	std::unique_ptr<CameraAnimation> cameraAnim;

	void SyncFreeCameraToCurrentView(const Viewer& viewer);

public:
	Controller();
	~Controller();

	// 입력/카메라 상태를 기본값으로 되돌린다.
	void Reset();
	// 씬 설정의 카메라 VMD를 로드한다.
	void LoadCameraAnim(const std::filesystem::path& cameraAnimPath);
	// 음악 재생 위치와 프레임 시간을 기준으로 애니메이션 시간을 진행한다.
	void StepTime(Viewer& viewer, Sound& music, std::chrono::steady_clock::time_point& saveTime) const;
	// 키보드와 마우스 입력을 처리해 재생/카메라 상태를 변경한다.
	void HandleInput(const Viewer& viewer, Sound& music);
	// 모션 카메라 또는 자유 카메라 상태로 뷰 행렬을 갱신한다.
	void UpdateCamera(Viewer& viewer) const;
};
