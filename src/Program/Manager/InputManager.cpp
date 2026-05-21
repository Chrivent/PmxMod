#include "InputManager.h"

#include "../../Viewer/Viewer.h"

namespace Chrivent {
	void InputManager::Reset() {
		state = {};
		prevSpaceDown = false;
		prevRDown = false;
		prevRightMouseDown = false;
		prevCursorX = 0.0;
		prevCursorY = 0.0;
	}

	void InputManager::Update(const ViewerInfo& viewerInfo) {
		state = {};
		const bool spaceDown = glfwGetKey(viewerInfo.window, GLFW_KEY_SPACE) == GLFW_PRESS;
		state.togglePause = spaceDown && !prevSpaceDown;
		prevSpaceDown = spaceDown;

		const bool rDown = glfwGetKey(viewerInfo.window, GLFW_KEY_R) == GLFW_PRESS;
		state.toggleCameraMode = rDown && !prevRDown;
		prevRDown = rDown;

		state.moveForward = glfwGetKey(viewerInfo.window, GLFW_KEY_W) == GLFW_PRESS;
		state.moveBackward = glfwGetKey(viewerInfo.window, GLFW_KEY_S) == GLFW_PRESS;
		state.moveLeft = glfwGetKey(viewerInfo.window, GLFW_KEY_A) == GLFW_PRESS;
		state.moveRight = glfwGetKey(viewerInfo.window, GLFW_KEY_D) == GLFW_PRESS;
		state.moveDown = glfwGetKey(viewerInfo.window, GLFW_KEY_Q) == GLFW_PRESS;
		state.moveUp = glfwGetKey(viewerInfo.window, GLFW_KEY_E) == GLFW_PRESS;

		state.rotateCamera = glfwGetMouseButton(viewerInfo.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
		double cursorX = 0.0;
		double cursorY = 0.0;
		glfwGetCursorPos(viewerInfo.window, &cursorX, &cursorY);
		if (state.rotateCamera && prevRightMouseDown)
			state.mouseDelta = glm::vec2(cursorX - prevCursorX, cursorY - prevCursorY);
		prevCursorX = cursorX;
		prevCursorY = cursorY;
		prevRightMouseDown = state.rotateCamera;
	}
}
