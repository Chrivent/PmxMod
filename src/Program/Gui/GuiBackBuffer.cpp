#include "GuiBackBuffer.h"

#include <algorithm>

namespace Chrivent {
	GuiBackBuffer::GuiBackBuffer(const HDC target, const RECT& area)
		: targetDc(target),
		  width((std::max)(0L, area.right - area.left)),
		  height((std::max)(0L, area.bottom - area.top)) {
		if (!targetDc || width == 0 || height == 0)
			return;
		memoryDc = CreateCompatibleDC(targetDc);
		if (!memoryDc)
			return;
		bitmap = CreateCompatibleBitmap(targetDc, width, height);
		if (!bitmap) {
			DeleteDC(memoryDc);
			memoryDc = nullptr;
			return;
		}
		previousBitmap = SelectObject(memoryDc, bitmap);
	}

	GuiBackBuffer::~GuiBackBuffer() {
		if (memoryDc && previousBitmap)
			SelectObject(memoryDc, previousBitmap);
		if (bitmap)
			DeleteObject(bitmap);
		if (memoryDc)
			DeleteDC(memoryDc);
	}

	void GuiBackBuffer::Present() const {
		if (targetDc && memoryDc && width > 0 && height > 0)
			BitBlt(targetDc, 0, 0, width, height, memoryDc, 0, 0, SRCCOPY);
	}
}
