#pragma once

#include <windows.h>
#include <filesystem>

#include <glm/glm.hpp>

#include "Config.h"
#include "Panel/ScenePanel.h"
#include "Panel/SoundPanel.h"

namespace Chrivent {
	struct CameraAnimation;
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
		SceneConfig sceneConfigStorage;
		ScenePanel scenePanel;
		SoundPanel soundPanel;
		HWND controlWindow = nullptr;

		// 현재 모션 카메라 시점에서 자유 카메라 위치와 회전값을 동기화한다.
		void SyncFreeCameraToCurrentView(const Viewer& viewer);
		// 컨트롤 창 크기에 맞춰 내부 윈도우 컨트롤 배치를 갱신한다.
		void ResizeControlWindow();
		// 컨트롤 창의 Win32 메시지를 처리한다.
		static LRESULT CALLBACK ControlWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	public:
		SceneConfig& sceneConfig;
	
		Controller();
		~Controller();

		// 외부에서 전달된 씬 설정을 컨트롤러 상태에 반영한다.
		void ApplySceneConfig(const SceneConfig& cfg);
		// 입력, 카메라, 씬 로드 상태를 기본값으로 초기화한다.
		void Reset();
		// 씬 열기/저장을 위한 컨트롤 창을 생성하거나 다시 표시한다.
		bool OpenControlWindow();
		// 컨트롤 창에 쌓인 Win32 메시지를 처리한다.
		void PollControlWindow() const;
		// 컨트롤 창을 파괴하고 관련 핸들을 초기화한다.
		void DestroyControlWindow();
		// 씬 설정 변경 플래그를 반환하고 초기화한다.
		bool ConsumeSceneConfigDirty();
		// 컨트롤 창의 사운드 패널이 조절할 사운드 객체를 연결한다.
		void BindSound(Sound& sound);
		// 카메라 VMD 파일을 읽어 모션 카메라 애니메이션을 준비한다.
		void LoadCameraAnim(const std::filesystem::path& cameraAnimPath);
		// 일시정지와 사운드 동기화를 반영해 뷰어 애니메이션 시간을 갱신한다.
		void StepTime(Viewer& viewer, Sound& music, std::chrono::steady_clock::time_point& saveTime) const;
		// 키보드와 마우스 입력을 처리해 재생 상태와 자유 카메라 상태를 갱신한다.
		void HandleInput(const Viewer& viewer, Sound& music);
		// 현재 카메라 모드에 맞춰 뷰어의 view/projection 행렬을 갱신한다.
		void UpdateCamera(Viewer& viewer) const;
	};
}
