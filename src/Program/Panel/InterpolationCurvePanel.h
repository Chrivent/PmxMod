#pragma once

#include "Core/Animation/Bezier.h"
#include "Program/Panel/Panel.h"

#include <string>
#include <vector>

namespace Chrivent {
	struct InterpolationCurveChannel {
		std::wstring name;
		std::vector<BezierControlPoints> curves;
	};

	struct InterpolationSelection {
		std::size_t selectedKeyCount = 0;
		std::vector<InterpolationCurveChannel> channels;
	};

	class InterpolationCurvePanel final : public Panel {
		HWND graphWindow = nullptr;
		InterpolationSelection selection;
		int selectedChannel = -1;
		int selectedCurve = -1;
		int selectedControlPoint = -1;

		// 보간 곡선 그래프 컨트롤의 Win32 메시지를 처리한다.
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		// 클릭한 보간 제어점을 선택하고 빈 영역이면 선택을 해제한다.
		void SelectControlPoint(int x, int y);
		// 현재 그래프 영역에 눈금, 제어점, 보간 곡선을 그린다.
		void Paint(HDC deviceContext) const;

	public:
		InterpolationCurvePanel() = default;

		// 부모 윈도우 아래에 보간 곡선 그래프를 생성한다.
		void Create(HWND parent) override;
		// 패널 영역 안에 정사각형 보간 곡선 그래프를 배치한다.
		void Resize(const RECT& clientRect) override;
		// 보간 곡선 그래프 컨트롤을 정리한다.
		void Destroy() override;
		// 선택된 키와 보간 곡선 데이터를 교체하고 제어점 선택을 초기화한다.
		void ApplySelection(InterpolationSelection interpolationSelection);
	};
}
