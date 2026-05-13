#include "Controller.h"

#include "Viewer.h"

#include "../src/Animation.h"
#include "../src/Reader.h"
#include "../src/Sound.h"

#include <iostream>

Controller::Controller() {
	Reset();
}

Controller::~Controller() = default;

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
	auto dt = static_cast<float>(elapsedSeconds);
	float t = viewer.animTime + dt;
	if (music.hasSound) {
		float adt = 0.0f;
		float at = 0.0f;
		music.PullTimes(adt, at);
		if (adt < 0.f)
			adt = 0.f;
		dt = adt;
		t = at;
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
