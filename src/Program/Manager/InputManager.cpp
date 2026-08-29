#include "Program/Manager/InputManager.h"

#include "Viewer/Viewer/Viewer.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Chrivent {
	void InputManager::ScrollCallback(GLFWwindow* sourceWindow, const double, const double yOffset) {
		if (auto* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(sourceWindow)))
			inputManager->pendingWheelDelta += yOffset;
	}

	void InputManager::AttachWindow(GLFWwindow* sourceWindow) {
		pendingWheelDelta = 0.0;
		if (!sourceWindow)
			return;
		glfwSetWindowUserPointer(sourceWindow, this);
		glfwSetScrollCallback(sourceWindow, ScrollCallback);
	}

	void InputManager::Reset() {
		state = {};
		prevRightMouseDown = false;
		prevCursorX = 0.0;
		prevCursorY = 0.0;
		pendingWheelDelta = 0.0;
	}

	void InputManager::Update(const Viewer& viewer) {
		state = {};
		GLFWwindow* window = viewer.GetWindow();
		state.moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
		state.moveBackward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
		state.moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
		state.moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
		state.moveDown = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
		state.moveUp = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
		state.rotateCamera = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
		double cursorX = 0.0;
		double cursorY = 0.0;
		glfwGetCursorPos(window, &cursorX, &cursorY);
		if (state.rotateCamera && prevRightMouseDown) {
			state.mouseDelta = glm::vec2(
				static_cast<float>(cursorX - prevCursorX), static_cast<float>(cursorY - prevCursorY));
		}
		prevCursorX = cursorX;
		prevCursorY = cursorY;
		prevRightMouseDown = state.rotateCamera;
		state.wheelDelta = static_cast<float>(pendingWheelDelta);
		pendingWheelDelta = 0.0;
	}
}
