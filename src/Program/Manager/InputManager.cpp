#include "InputManager.h"

#include "../../Viewer/Viewer.h"

namespace Chrivent {
	void InputManager::ScrollCallback(GLFWwindow*, const double, const double yOffset) {
		if (activeManager)
			activeManager->pendingWheelDelta += yOffset;
	}

	void InputManager::AttachWindow(GLFWwindow* sourceWindow) {
		activeManager = this;
		pendingWheelDelta = 0.0;
		if (!sourceWindow)
			return;
		glfwSetScrollCallback(sourceWindow, ScrollCallback);
	}

	void InputManager::Reset() {
		state = {};
		prevSpaceDown = false;
		prevRightMouseDown = false;
		prevCursorX = 0.0;
		prevCursorY = 0.0;
		pendingWheelDelta = 0.0;
	}

	void InputManager::Update(const Viewer& viewer) {
		state = {};
		const bool spaceDown = glfwGetKey(viewer.window, GLFW_KEY_SPACE) == GLFW_PRESS;
		state.togglePause = spaceDown && !prevSpaceDown;
		prevSpaceDown = spaceDown;
		state.moveForward = glfwGetKey(viewer.window, GLFW_KEY_W) == GLFW_PRESS;
		state.moveBackward = glfwGetKey(viewer.window, GLFW_KEY_S) == GLFW_PRESS;
		state.moveLeft = glfwGetKey(viewer.window, GLFW_KEY_A) == GLFW_PRESS;
		state.moveRight = glfwGetKey(viewer.window, GLFW_KEY_D) == GLFW_PRESS;
		state.moveDown = glfwGetKey(viewer.window, GLFW_KEY_Q) == GLFW_PRESS;
		state.moveUp = glfwGetKey(viewer.window, GLFW_KEY_E) == GLFW_PRESS;
		state.rotateCamera = glfwGetMouseButton(viewer.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
		double cursorX = 0.0;
		double cursorY = 0.0;
		glfwGetCursorPos(viewer.window, &cursorX, &cursorY);
		if (state.rotateCamera && prevRightMouseDown)
			state.mouseDelta = glm::vec2(cursorX - prevCursorX, cursorY - prevCursorY);
		prevCursorX = cursorX;
		prevCursorY = cursorY;
		prevRightMouseDown = state.rotateCamera;
		state.wheelDelta = pendingWheelDelta;
		pendingWheelDelta = 0.0;
	}
}
