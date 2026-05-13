#pragma once

#include <vector>
#include <windows.h>
#include <filesystem>
#include <glm/glm.hpp>

class CameraAnimation;
class Sound;
class Viewer;

struct ModelConfig {
	std::filesystem::path modelPath;
	std::vector<std::filesystem::path> animPaths;
	float scale = 1.0f;
};

struct SceneConfig {
	std::vector<ModelConfig> modelConfigs;
	std::filesystem::path cameraAnim;
	std::filesystem::path musicPath;
};

class Controller {
	static constexpr int kOpenButtonId = 1001;
	static constexpr int kSaveButtonId = 1002;

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
	SceneConfig sceneConfig;
	std::filesystem::path sceneFilePath;
	HWND controlWindow = nullptr;
	HWND statusText = nullptr;

	void SyncFreeCameraToCurrentView(const Viewer& viewer);
	void ResizeControlWindow() const;
	void SetStatusText(const std::wstring& text) const;
	bool SaveSceneConfig(const std::filesystem::path& filepath) const;
	bool LoadSceneConfig(const std::filesystem::path& filepath);
	void ShowOpenSceneDialog();
	void ShowSaveSceneDialog();

	static LRESULT CALLBACK ControlWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	Controller();
	~Controller();

	void SetSceneConfig(const SceneConfig& cfg);
	void Reset();
	bool OpenControlWindow();
	void PollControlWindow() const;
	void DestroyControlWindow();
	void LoadCameraAnim(const std::filesystem::path& cameraAnimPath);
	void StepTime(Viewer& viewer, Sound& music, std::chrono::steady_clock::time_point& saveTime) const;
	void HandleInput(const Viewer& viewer, Sound& music);
	void UpdateCamera(Viewer& viewer) const;
};
