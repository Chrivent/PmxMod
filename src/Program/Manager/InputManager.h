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
		// ?댁쟾 ?꾨젅???낅젰 ?곹깭瑜?珥덇린?뷀븳??
		void Reset();
		// ?꾩옱 GLFW ?낅젰???쎌뼱 ?대쾲 ?꾨젅???낅젰 ?곹깭濡?蹂?섑븳??
		void Update(const Viewer& viewer);
		// ?대쾲 ?꾨젅?꾩뿉 怨꾩궛???낅젰 ?곹깭瑜?諛섑솚?쒕떎.
		const InputState& GetState() const { return state; }
	};
}
