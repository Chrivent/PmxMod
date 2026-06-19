#pragma once

#include <windows.h>

namespace Chrivent {
	class Panel {
	public:
		virtual ~Panel() = default;

		// 소유 윈도우의 메뉴에 패널 명령을 추가한다.
		virtual void AddMenu(HMENU menu) {}
		// 부모 윈도우 아래에 패널 컨트롤을 생성한다.
		virtual void Create(HWND parent) {}
		// 패널 클라이언트 영역 크기에 맞춰 컨트롤 배치를 갱신한다.
		virtual void Resize(const RECT& clientRect) {}
		// 버튼이나 메뉴에서 들어온 명령을 처리한다.
		virtual bool HandleCommand(int commandId, int notificationCode) { return false; }
		// 슬라이더 등 스크롤 컨트롤에서 들어온 변경을 처리한다.
		virtual bool HandleScroll(HWND control, int scrollCode) { return false; }
		// 패널 윈도우와 보유 컨트롤을 해제한다.
		virtual void Destroy() {}
	};
}
