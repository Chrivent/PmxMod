#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;

namespace Chrivent {
	struct ViewerInfo;

	struct InputState {
		bool togglePause = false;
		bool moveForward = false;
		bool moveBackward = false;
		bool moveLeft = false;
		bool moveRight = false;
		bool moveDown = false;
		bool moveUp = false;
		bool rotateCamera = false;
		glm::vec2 mouseDelta = glm::vec2(0.0f);
		float wheelDelta = 0.0f;
	};

	class InputManager {
		inline static InputManager* activeManager = nullptr;

		InputState state;
		bool prevSpaceDown = false;
		bool prevRightMouseDown = false;
		double prevCursorX = 0.0;
		double prevCursorY = 0.0;
		double pendingWheelDelta = 0.0;

		// GLFW 스크롤 입력을 다음 입력 갱신까지 누적한다.
		static void ScrollCallback(GLFWwindow*, double, double yOffset);

	public:
		const InputState& GetState() const { return state; }
		
		// 입력을 받을 렌더링 창에 스크롤 콜백을 연결한다.
		void AttachWindow(GLFWwindow* sourceWindow);
		// 이전 프레임 입력 상태를 초기화한다.
		void Reset();
		// 현재 GLFW 입력을 읽어 이번 프레임 입력 상태로 변환한다.
		void Update(const ViewerInfo& viewerInfo);
	};
}
