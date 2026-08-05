#include "Core/Animation/Bezier.h"

#include <algorithm>

namespace Chrivent {
	float Bezier::EvaluateBezier(const float t, const float p1, const float p2) {
		const float it = 1.0f - t;
		return 3.0f * it * it * t * p1 + 3.0f * it * t * t * p2 + t * t * t;
	}

	float Bezier::FindBezierX(float time, const float x1, const float x2) {
		time = std::clamp(time, 0.0f, 1.0f);
		float start = 0.0f;
		float stop = 1.0f;
		float t = 0.5f;
		for (int iteration = 0; iteration < 32; iteration++) {
			const float x = EvaluateBezier(t, x1, x2);
			const float diff = time - x;
			if (std::abs(diff) < 1.0e-5f)
				return t;
			if (diff < 0.0f)
				stop = t;
			else
				start = t;
			t = (start + stop) * 0.5f;
		}
		return t;
	}
	
	void Bezier::Assign(const int x0, const int x1, const int y0, const int y1) {
		const auto Normalize = [](const int value) { return value / 127.0f; };
		controlPoints.p1 = glm::vec2(Normalize(x0), Normalize(y0));
		controlPoints.p2 = glm::vec2(Normalize(x1), Normalize(y1));
	}

	float Bezier::Evaluate(const float time) const {
		if (time <= 0.0f)
			return 0.0f;
		if (time >= 1.0f)
			return 1.0f;
		const float bezierParameter = FindBezierX(time, controlPoints.p1.x, controlPoints.p2.x);
		return EvaluateBezier(bezierParameter, controlPoints.p1.y, controlPoints.p2.y);
	}
}
