#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	class Viewer;

	struct InputState {
		bool togglePause = false;
		bool toggleCameraMode = false;
		bool moveForward = false;
		bool moveBackward = false;
		bool moveLeft = false;
		bool moveRight = false;
		bool moveDown = false;
		bool moveUp = false;
		bool rotateCamera = false;
		glm::vec2 mouseDelta = glm::vec2(0.0f);
	};

	class InputManager {
		InputState state;
		bool prevSpaceDown = false;
		bool prevRDown = false;
		bool prevRightMouseDown = false;
		double prevCursorX = 0.0;
		double prevCursorY = 0.0;

	public:
		const InputState& GetState() const { return state; }
		
		// 이전 프레임 입력 상태를 초기화한다.
		void Reset();
		// 현재 GLFW 입력을 읽어 이번 프레임 입력 상태로 변환한다.
		void Update(const Viewer& viewer);
	};
}
