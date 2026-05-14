#include "Controller.h"

#include "../Viewer/Viewer.h"

#include "../Animation/CameraAnimation.h"
#include "../Program/Sound.h"

#include <iostream>

namespace Chrivent {
	Controller::Controller() {
		Reset();
	}

	Controller::~Controller() {
		DestroyControlWindow();
	}

	void Controller::ApplySceneConfig(const SceneConfig& cfg) {
		sceneConfig = cfg;
		sceneFilePath.clear();
		sceneConfigDirty = false;
	}

	void Controller::Reset() {
		paused = false;
		prevSpaceDown = false;
		useMotionCamera = true;
		hasFreeCameraState = false;
		prevRDown = false;
		prevRightMouseDown = false;
		prevCursorX = 0.0;
		prevCursorY = 0.0;
		freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
		freeCamYaw = glm::radians(-90.0f);
		freeCamPitch = 0.0f;
		cameraAnim.reset();
		sceneConfigDirty = false;
	}

	bool Controller::OpenControlWindow() {
		if (controlWindow) {
			ShowWindow(controlWindow, SW_SHOWNORMAL);
			return true;
		}
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = ControlWindowProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.lpszClassName = L"PmxModControllerWindow";
		RegisterClassExW(&wc);
		controlWindow = CreateWindowExW(
			0, L"PmxModControllerWindow", L"PmxMod Controller",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 420, 220,
			nullptr, nullptr, instance, this);
		if (!controlWindow)
			return false;
		ShowWindow(controlWindow, SW_SHOWNORMAL);
		UpdateWindow(controlWindow);
		return true;
	}

	void Controller::PollControlWindow() const {
		if (!controlWindow)
			return;
		MSG msg{};
		while (PeekMessageW(&msg, controlWindow, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	void Controller::DestroyControlWindow() {
		if (controlWindow)
			DestroyWindow(controlWindow);
		controlWindow = nullptr;
		statusText = nullptr;
	}

	void Controller::ResizeControlWindow() const {
		if (!controlWindow)
			return;
		RECT client{};
		GetClientRect(controlWindow, &client);
		constexpr int x = 14;
		constexpr int y = 14;
		const int width = static_cast<int>(client.right) - x * 2;
		if (statusText)
			MoveWindow(statusText, x, y, width, 64, TRUE);
	}

	void Controller::SetStatusText(const std::wstring& text) const {
		if (statusText)
			SetWindowTextW(statusText, text.c_str());
	}

	bool Controller::SaveSceneConfig(const std::filesystem::path& filepath) const {
		return sceneConfig.Save(filepath);
	}

	bool Controller::LoadSceneConfig(const std::filesystem::path& filepath) {
		if (!sceneConfig.Load(filepath))
			return false;
		sceneFilePath = filepath;
		sceneConfigDirty = true;
		return true;
	}

	void Controller::ShowOpenSceneDialog() {
		std::vector filename(MAX_PATH, L'\0');
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = controlWindow;
		ofn.lpstrFilter = L"PmxMod Scene (*.pms)\0*.pms\0All Files (*.*)\0*.*\0";
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = static_cast<DWORD>(filename.size());
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"pms";
		if (!GetOpenFileNameW(&ofn))
			return;
		if (LoadSceneConfig(filename.data()))
			SetStatusText(L"Scene config loaded.");
		else
			SetStatusText(L"Failed to load scene config.");
	}

	void Controller::ShowSaveSceneDialog() {
		std::vector<wchar_t> filename(MAX_PATH, L'\0');
		if (!sceneFilePath.empty()) {
			const auto native = sceneFilePath.wstring();
			std::wcsncpy(filename.data(), native.c_str(), filename.size() - 1);
		}
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = controlWindow;
		ofn.lpstrFilter = L"PmxMod Scene (*.pms)\0*.pms\0All Files (*.*)\0*.*\0";
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = static_cast<DWORD>(filename.size());
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"pms";
		if (!GetSaveFileNameW(&ofn))
			return;
		sceneFilePath = filename.data();
		if (SaveSceneConfig(sceneFilePath))
			SetStatusText(L"Current scene config saved.");
		else
			SetStatusText(L"Failed to save scene config.");
	}

	bool Controller::ConsumeSceneConfigDirty() {
		const bool dirty = sceneConfigDirty;
		sceneConfigDirty = false;
		return dirty;
	}

	LRESULT CALLBACK Controller::ControlWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		auto* controller = reinterpret_cast<Controller*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (msg == WM_NCCREATE) {
			const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
			controller = static_cast<Controller*>(create->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(controller));
			controller->controlWindow = hwnd;
		}
		if (!controller)
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		switch (msg) {
			case WM_CREATE: {
				const HMENU menu = CreateMenu();
				HMENU fileMenu = CreatePopupMenu();
				AppendMenuW(fileMenu, MF_STRING, kOpenButtonId, L"Open...");
				AppendMenuW(fileMenu, MF_STRING, kSaveButtonId, L"Save...");
				AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");
				SetMenu(hwnd, menu);
				controller->statusText = CreateWindowExW(
					0, L"STATIC", L"Open or save the current scene config.",
					WS_CHILD | WS_VISIBLE,
					0, 0, 0, 0,
					hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
				controller->ResizeControlWindow();
				return 0;
			}
			case WM_SIZE:
				controller->ResizeControlWindow();
				return 0;
			case WM_COMMAND:
				switch (LOWORD(wParam)) {
				case kOpenButtonId:
						controller->ShowOpenSceneDialog();
						return 0;
				case kSaveButtonId:
						controller->ShowSaveSceneDialog();
						return 0;
				default:
						break;
				}
				break;
			case WM_APP + 2:
				ShowWindow(hwnd, SW_SHOWNORMAL);
				SetForegroundWindow(hwnd);
				return 0;
			case WM_CLOSE:
				ShowWindow(hwnd, SW_HIDE);
				return 0;
			case WM_DESTROY:
				controller->controlWindow = nullptr;
				controller->statusText = nullptr;
				return 0;
			default:
				break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	void Controller::LoadCameraAnim(const std::filesystem::path& cameraAnimPath) {
		cameraAnim.reset();
		if (cameraAnimPath.empty()) {
			std::cout << "No camera VMD file.\n";
			return;
		}
		VmdReader camVmd;
		if (camVmd.ReadFile(cameraAnimPath.c_str()) && !camVmd.cameras.empty()) {
			auto vmdCamAnim = std::make_unique<CameraAnimation>();
			if (!vmdCamAnim->Create(camVmd))
				std::cout << "Failed to create VMDCameraAnimation.\n";
			cameraAnim = std::move(vmdCamAnim);
		}
	}

	void Controller::StepTime(Viewer& viewer, Sound& music, std::chrono::steady_clock::time_point& saveTime) const {
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

	void Controller::HandleInput(const Viewer& viewer, Sound& music) {
		const bool spaceDown = glfwGetKey(viewer.window, GLFW_KEY_SPACE) == GLFW_PRESS;
		if (spaceDown && !prevSpaceDown) {
			paused = !paused;
			if (paused)
				music.Pause();
			else
				music.Resume();
		}
		prevSpaceDown = spaceDown;
		const bool rDown = glfwGetKey(viewer.window, GLFW_KEY_R) == GLFW_PRESS;
		if (rDown && !prevRDown) {
			if (useMotionCamera && !hasFreeCameraState) {
				SyncFreeCameraToCurrentView(viewer);
				hasFreeCameraState = true;
			}
			useMotionCamera = !useMotionCamera;
			prevRightMouseDown = false;
		}
		prevRDown = rDown;
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
		if (glfwGetKey(viewer.window, GLFW_KEY_W) == GLFW_PRESS)
			freeCamPosition += forward * moveSpeed;
		if (glfwGetKey(viewer.window, GLFW_KEY_S) == GLFW_PRESS)
			freeCamPosition -= forward * moveSpeed;
		if (glfwGetKey(viewer.window, GLFW_KEY_A) == GLFW_PRESS)
			freeCamPosition -= right * moveSpeed;
		if (glfwGetKey(viewer.window, GLFW_KEY_D) == GLFW_PRESS)
			freeCamPosition += right * moveSpeed;
		if (glfwGetKey(viewer.window, GLFW_KEY_Q) == GLFW_PRESS)
			freeCamPosition -= up * moveSpeed;
		if (glfwGetKey(viewer.window, GLFW_KEY_E) == GLFW_PRESS)
			freeCamPosition += up * moveSpeed;
		const bool rightMouseDown = glfwGetMouseButton(viewer.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
		double cursorX = 0.0, cursorY = 0.0;
		glfwGetCursorPos(viewer.window, &cursorX, &cursorY);
		if (rightMouseDown) {
			if (prevRightMouseDown) {
				constexpr float mouseSensitivity = 0.0035f;
				const double dx = cursorX - prevCursorX;
				const double dy = cursorY - prevCursorY;
				freeCamYaw += static_cast<float>(dx) * mouseSensitivity;
				freeCamPitch -= static_cast<float>(dy) * mouseSensitivity;
				freeCamPitch = std::clamp(freeCamPitch, glm::radians(-89.0f), glm::radians(89.0f));
			}
			prevCursorX = cursorX;
			prevCursorY = cursorY;
		}
		prevRightMouseDown = rightMouseDown;
	}

	void Controller::UpdateCamera(Viewer& viewer) const {
		if (useMotionCamera && cameraAnim) {
			cameraAnim->Evaluate(viewer.animTime * 30.0f);
			const auto cam = cameraAnim->camera;
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

	void Controller::SyncFreeCameraToCurrentView(const Viewer& viewer) {
		const glm::mat4 invView = glm::inverse(viewer.viewMat);
		freeCamPosition = glm::vec3(invView[3]);
		const glm::vec3 forward = -glm::normalize(glm::vec3(invView[2]));
		freeCamYaw = std::atan2(forward.z, forward.x);
		freeCamPitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
	}
}
