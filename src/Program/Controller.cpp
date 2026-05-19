#include "Controller.h"

#include "../Viewer/Viewer.h"

#include "../Animation/Camera/CameraAnimation.h"
#include "../Animation/Camera/CameraAnimationBuilder.h"
#include "../Program/Sound.h"

#include <iostream>

namespace Chrivent {
	void Controller::SyncFreeCameraToCurrentView(const Viewer& viewer) {
		const glm::mat4 invView = glm::inverse(viewer.viewMat);
		freeCamPosition = glm::vec3(invView[3]);
		const glm::vec3 forward = -glm::normalize(glm::vec3(invView[2]));
		freeCamYaw = std::atan2(forward.z, forward.x);
		freeCamPitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
	}

	void Controller::ResizeControlWindow() {
		if (!controlWindow)
			return;
		RECT client{};
		GetClientRect(controlWindow, &client);
		scenePanel.Resize(client);
	}

	LRESULT CALLBACK Controller::ControlWindowProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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
				controller->scenePanel.AddMenu(menu);
				SetMenu(hwnd, menu);
				controller->scenePanel.Create(hwnd);
				controller->ResizeControlWindow();
				return 0;
			}
			case WM_SIZE:
				controller->ResizeControlWindow();
				return 0;
			case WM_COMMAND:
				if (controller->scenePanel.HandleCommand(LOWORD(wParam)))
					return 0;
				break;
			case WM_APP + 2:
				ShowWindow(hwnd, SW_SHOWNORMAL);
				SetForegroundWindow(hwnd);
				return 0;
			case WM_CLOSE:
				ShowWindow(hwnd, SW_HIDE);
				return 0;
			case WM_DESTROY:
				controller->scenePanel.Destroy();
				controller->soundPanel.Destroy();
				controller->controlWindow = nullptr;
				return 0;
			default:
				break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	Controller::Controller()
		: scenePanel(sceneConfigStorage), sceneConfig(sceneConfigStorage) {
		Reset();
	}

	Controller::~Controller() {
		DestroyControlWindow();
	}

	void Controller::ApplySceneConfig(const SceneConfig& cfg) {
		scenePanel.ApplySceneConfig(cfg);
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
		scenePanel.Reset();
	}

	bool Controller::OpenControlWindow() {
		if (controlWindow) {
			ShowWindow(controlWindow, SW_SHOWNORMAL);
			soundPanel.Show();
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
		soundPanel.Show();
		return true;
	}

	void Controller::PollControlWindow() const {
		MSG msg{};
		if (controlWindow) {
			while (PeekMessageW(&msg, controlWindow, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
		soundPanel.Poll();
	}

	void Controller::DestroyControlWindow() {
		if (controlWindow)
			DestroyWindow(controlWindow);
		controlWindow = nullptr;
		scenePanel.Destroy();
		soundPanel.Destroy();
	}

	bool Controller::ConsumeSceneConfigDirty() {
		return scenePanel.ConsumeSceneConfigDirty();
	}

	void Controller::BindSound(Sound& sound) {
		soundPanel.BindSound(sound);
	}

	void Controller::LoadCameraAnim(const std::filesystem::path& cameraAnimPath) {
		cameraAnim.reset();
		if (cameraAnimPath.empty()) {
			std::cout << "No camera VMD file.\n";
			return;
		}
		VmdParser camVmd;
		if (camVmd.ReadFile(cameraAnimPath.c_str()) && !camVmd.cameras.empty()) {
			auto vmdCamAnim = std::make_unique<CameraAnimation>();
			const CameraAnimationBuilder cameraAnimationBuilder(*vmdCamAnim);
			if (!cameraAnimationBuilder.Add(camVmd))
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
}
