#pragma once

#include <Windows.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Chrivent {
	// 렌더링 창 위에 현재 FPS를 표시하는 프로그램 UI 오버레이를 관리한다.
	class FpsOverlay {
		GLFWwindow* ownerWindow = nullptr;
		HWND overlayWindow = nullptr;
		HFONT font = nullptr;

		// 오버레이 윈도우의 배경과 FPS 문자열을 그린다.
		static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
		// 렌더링 창 클라이언트 좌측 상단에 오버레이를 배치한다.
		void Position() const;

	public:
		FpsOverlay() = default;
		~FpsOverlay();
		FpsOverlay(const FpsOverlay&) = delete;
		FpsOverlay& operator=(const FpsOverlay&) = delete;
		FpsOverlay(FpsOverlay&&) = delete;
		FpsOverlay& operator=(FpsOverlay&&) = delete;

		// 지정한 GLFW 렌더링 창 위에 FPS 오버레이를 생성한다.
		void Create(GLFWwindow* sourceWindow);
		// 오버레이 윈도우와 글꼴 리소스를 해제한다.
		void Reset();
		// 현재 측정한 FPS 문자열을 갱신한다.
		void Update(double fps) const;
		// 오버레이의 표시 상태와 위치를 갱신한다.
		void SetVisible(bool visible) const;
	};
}
