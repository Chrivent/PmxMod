#pragma once

#include <windows.h>

namespace Chrivent {
	// GUI 창의 깜빡임을 줄이는 메모리 백버퍼를 관리한다.
	class GuiBackBuffer {
		HDC targetDc = nullptr;
		HDC memoryDc = nullptr;
		HBITMAP bitmap = nullptr;
		HGDIOBJ previousBitmap = nullptr;
		int width = 0;
		int height = 0;

	public:
		GuiBackBuffer(HDC target, const RECT& area);
		~GuiBackBuffer();

		GuiBackBuffer(const GuiBackBuffer&) = delete;
		GuiBackBuffer& operator=(const GuiBackBuffer&) = delete;
		GuiBackBuffer(GuiBackBuffer&&) = delete;
		GuiBackBuffer& operator=(GuiBackBuffer&&) = delete;

		HDC GetDc() const { return memoryDc; }

		// 메모리 DC에 그린 결과를 대상 DC로 한 번에 복사한다.
		void Present() const;
	};
}
