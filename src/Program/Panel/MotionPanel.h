#pragma once

#include "Panel.h"

namespace Chrivent {
	class MotionPanel final : public Panel {
		HWND titleText = nullptr;

	public:
		MotionPanel() = default;

		// 부모 윈도우 아래에 모션 패널 컨트롤을 생성한다.
		void Create(HWND parent) override;
		// 패널 크기에 맞춰 모션 패널 컨트롤 배치를 갱신한다.
		void Resize(const RECT& clientRect) override;
		// 모션 패널 컨트롤 핸들을 정리한다.
		void Destroy() override;
	};
}
