#pragma once

#include <windows.h>
#include <CommCtrl.h>

namespace Chrivent {
	class Panel {
		bool inputLocked = false;

	protected:
		// 입력 잠금 중 컨트롤에 전달하지 않을 마우스와 키보드 메시지인지 확인한다.
		static bool IsInputLockMessage(UINT msg);
		// 입력 잠금 대상 컨트롤의 Win32 메시지를 처리한다.
		static LRESULT CALLBACK InputLockedControlWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
			UINT_PTR subclassId, DWORD_PTR data);
		// 입력 잠금 대상 컨트롤에 공통 subclass를 연결한다.
		void AttachInputLockedControl(const HWND control, const UINT_PTR subclassId) {
			SetWindowSubclass(control, InputLockedControlWindowProc, subclassId, reinterpret_cast<DWORD_PTR>(this));
		}
		// 패널이 소유한 입력 잠금 컨트롤들의 잠금 상태를 갱신하고 변경 여부를 반환한다.
		bool ApplyInputLock(bool locked);
		bool IsInputLocked() const { return inputLocked; }

	public:
		virtual ~Panel() = default;

		// 소유 윈도우의 메뉴에 패널 명령을 추가한다.
		virtual void AddMenu(HMENU menu) {}
		// 부모 윈도우 아래에 패널 컨트롤을 생성한다.
		virtual void Create(HWND parent) {}
		// 패널 클라이언트 영역 크기에 맞춰 컨트롤 배치를 갱신한다.
		virtual void Resize(const RECT& clientRect) {}
		// 현재 언어에 맞춰 패널의 고정 GUI 문구를 갱신한다.
		virtual void UpdateLanguage() {}
		// 버튼이나 메뉴에서 들어온 명령을 처리한다.
		virtual bool HandleCommand(UINT_PTR commandId, int notificationCode) { return false; }
		// 리스트뷰 등 공용 컨트롤에서 들어온 알림을 처리하고 필요한 반환값을 채운다.
		virtual bool HandleNotify(const NMHDR& notifyHeader, LRESULT& result) { return false; }
		// 슬라이더 등 스크롤 컨트롤에서 들어온 변경을 처리한다.
		virtual bool HandleScroll(HWND control, int scrollCode) { return false; }
		// 패널 윈도우와 보유 컨트롤을 해제한다.
		virtual void Destroy() {}
	};
}
