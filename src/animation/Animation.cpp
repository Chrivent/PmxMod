#include "Animation.h"

#include <algorithm>

void Animation::AssignBezier(std::pair<glm::vec2, glm::vec2>& bezier, const int x0, const int x1, const int y0, const int y1) {
	bezier.first = glm::vec2(static_cast<float>(x0) / 127.0f, static_cast<float>(y0) / 127.0f);
	bezier.second = glm::vec2(static_cast<float>(x1) / 127.0f, static_cast<float>(y1) / 127.0f);
}

float Animation::Bezier(const float t, const float p1, const float p2) {
	const float it = 1.0f - t;
	return 3.0f * it * it * t * p1 + 3.0f * it * t * t * p2 + t * t * t;
}

float Animation::FindBezierX(float time, const float x1, const float x2) {
	time = std::clamp(time, 0.0f, 1.0f);
	float start = 0.0f, stop = 1.0f;
	float t = 0.5f;
	while (true) {
		const float x = Bezier(t, x1, x2);
		const float diff = time - x;
		if (std::abs(diff) < 1e-5f)
			break;
		(diff < 0.0f ? stop : start) = t;
		t = (start + stop) * 0.5f;
	}
	return t;
}
