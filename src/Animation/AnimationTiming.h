#pragma once

namespace Chrivent {
	struct AnimationTiming {
		static constexpr float motionFps = 30.0f;
		static constexpr float renderFps = 60.0f;
		static constexpr float renderSecondsPerFrame = 1.0f / renderFps;
	};
}
