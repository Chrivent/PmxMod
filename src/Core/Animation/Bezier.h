#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	// 베지어 보간 곡선의 두 제어점을 보관한다.
	struct BezierControlPoints {
		glm::vec2 p1;
		glm::vec2 p2;
	};

	// MMD 베지어 제어점으로 정규화된 보간값을 계산한다.
	class Bezier {
		BezierControlPoints controlPoints;

		// 3차 Bezier 곡선의 단일 축 값을 계산한다.
		static float EvaluateBezier(float t, float p1, float p2);
		// 주어진 x 시간에 대응하는 Bezier 매개변수 t를 이분 탐색으로 찾는다.
		static float FindBezierX(float time, float x1, float x2);
		
	public:
		const BezierControlPoints& GetControlPoints() const { return controlPoints; }

		// VMD 보간 바이트 네 개를 정규화된 2D Bezier 제어점으로 변환한다.
		void Assign(int x0, int x1, int y0, int y1);
		// 주어진 x 시간에 대응하는 Bezier 보간 값을 계산한다.
		float Evaluate(float time) const;
	};
}
