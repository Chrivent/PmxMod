#include "Program/Panel/MotionPanel.h"

#include "Program/Gui/GuiBackBuffer.h"
#include "Program/Gui/GuiDrawer.h"
#include "Program/Gui/GuiTheme.h"
#include "Program/Language.h"
#include "Program/Sound.h"

#include <CommCtrl.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <windowsx.h>

namespace Chrivent {
	LRESULT CALLBACK MotionPanel::WindowProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
		auto* panel = reinterpret_cast<MotionPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (msg == WM_NCCREATE) {
			const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
			panel = static_cast<MotionPanel*>(create->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
			panel->timelineWindow = hwnd;
		}
		if (!panel)
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		switch (msg) {
		case WM_SIZE:
			panel->UpdateVerticalScrollBar();
			panel->UpdateHorizontalScrollBar();
			InvalidateRect(hwnd, nullptr, TRUE);
			return 0;
		case WM_VSCROLL:
			panel->ScrollRows(LOWORD(wParam), HIWORD(wParam));
			return 0;
		case WM_HSCROLL:
			panel->ScrollFrames(LOWORD(wParam), HIWORD(wParam));
			return 0;
		case WM_MOUSEWHEEL:
			panel->ScrollRows(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
			return 0;
		case WM_LBUTTONDOWN: {
			const int x = GET_X_LPARAM(lParam);
			const int y = GET_Y_LPARAM(lParam);
			if (y >= kHeaderHeight && y < panel->ResolveTimelineBottom()) {
				const int visibleRow = panel->firstRow + (y - kHeaderHeight) / kRowHeight;
				if (x < kLabelWidth)
					panel->ToggleGroup(visibleRow);
				else {
					panel->selectionStart = {x, y};
					panel->selectionEnd = panel->selectionStart;
					panel->selectingKeys = true;
					SetCapture(hwnd);
				}
			}
			return 0;
		}
		case WM_MOUSEMOVE:
			if (panel->selectingKeys && (wParam & MK_LBUTTON)) {
				panel->selectionEnd = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
				InvalidateRect(hwnd, nullptr, FALSE);
			}
			return 0;
		case WM_LBUTTONUP:
			if (panel->selectingKeys) {
				panel->selectionEnd = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
				panel->selectingKeys = false;
				ReleaseCapture();
				const bool additive = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
				const int width = std::abs(panel->selectionEnd.x - panel->selectionStart.x);
				const int height = std::abs(panel->selectionEnd.y - panel->selectionStart.y);
				if (width > 3 || height > 3)
					panel->SelectKeysInRectangle(additive);
				else {
					const int visibleRow = panel->firstRow + (panel->selectionStart.y - kHeaderHeight) / kRowHeight;
					if (!panel->SelectKey(visibleRow, panel->selectionStart.x, additive)) {
						panel->ClearKeySelection();
						panel->interpolationSelectionDirty = true;
					}
				}
				InvalidateRect(hwnd, nullptr, FALSE);
			}
			return 0;
		case WM_CAPTURECHANGED:
			panel->selectingKeys = false;
			return 0;
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT: {
			PAINTSTRUCT paint{};
			const HDC targetDc = BeginPaint(hwnd, &paint);
			RECT client{};
			GetClientRect(hwnd, &client);
			const GuiBackBuffer backBuffer(targetDc, client);
			if (const HDC memoryDc = backBuffer.GetDc()) {
				panel->Paint(memoryDc);
				backBuffer.Present();
			} else
				panel->Paint(targetDc);
			EndPaint(hwnd, &paint);
			return 0;
		}
		case WM_DESTROY:
			panel->timelineWindow = nullptr;
			return 0;
		default:
			break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	LRESULT CALLBACK MotionPanel::EditWindowProc(
		const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam, const UINT_PTR subclassId, const DWORD_PTR data) {
		if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
			reinterpret_cast<MotionPanel*>(data)->ApplyInputFrame();
			return 0;
		}
		if (msg == WM_NCDESTROY)
			RemoveWindowSubclass(hwnd, EditWindowProc, subclassId);
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	void MotionPanel::ApplyInputFrame() {
		if (updatingFrameEdit || !frameEdit)
			return;
		wchar_t text[16]{};
		GetWindowTextW(frameEdit, text, std::size(text));
		if (text[0] == L'\0')
			return;
		currentFrame = std::clamp(_wtoi(text), 0, kMaxEditableFrame);
		seekFrame = currentFrame;
		seekRequested = true;
		seekFinished = true;
		FollowCurrentFrame();
		SetScrollPos(timelineWindow, SB_HORZ, std::min(currentFrame, totalFrame), TRUE);
		InvalidateRect(timelineWindow, nullptr, FALSE);
	}

	void MotionPanel::ClampInputFrameToScrollRange() {
		if (currentFrame <= totalFrame)
			return;
		currentFrame = totalFrame;
		seekFrame = currentFrame;
		seekRequested = true;
		seekFinished = true;
		UpdateFrameEditText();
		FollowCurrentFrame();
		SetScrollPos(timelineWindow, SB_HORZ, currentFrame, TRUE);
		InvalidateRect(timelineWindow, nullptr, FALSE);
	}

	void MotionPanel::UpdateFrameEditText(const bool force) {
		if (!frameEdit || (!force && GetFocus() == frameEdit))
			return;
		updatingFrameEdit = true;
		SetWindowTextW(frameEdit, std::to_wstring(currentFrame).c_str());
		updatingFrameEdit = false;
	}

	void MotionPanel::UpdateVerticalScrollBar() const {
		if (!timelineWindow)
			return;
		RECT client{};
		GetClientRect(timelineWindow, &client);
		const int visibleRows = std::max(1, (ResolveTimelineBottom() - kHeaderHeight) / kRowHeight);
		SCROLLINFO vertical{};
		vertical.cbSize = sizeof(vertical);
		vertical.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		vertical.nMin = 0;
		vertical.nMax = std::max(0, CalculateVisibleRowCount() - 1);
		vertical.nPage = visibleRows;
		vertical.nPos = firstRow;
		SetScrollInfo(timelineWindow, SB_VERT, &vertical, TRUE);
	}

	void MotionPanel::UpdateHorizontalScrollBar() const {
		if (!timelineWindow)
			return;
		SCROLLINFO horizontal{};
		horizontal.cbSize = sizeof(horizontal);
		horizontal.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
		horizontal.nMin = 0;
		horizontal.nMax = totalFrame;
		horizontal.nPage = 1;
		horizontal.nPos = std::min(currentFrame, totalFrame);
		SetScrollInfo(timelineWindow, SB_HORZ, &horizontal, TRUE);
		ShowScrollBar(timelineWindow, SB_HORZ, TRUE);
	}

	int MotionPanel::ResolveTimelineBottom() const {
		if (!timelineWindow)
			return kHeaderHeight;
		RECT client{};
		GetClientRect(timelineWindow, &client);
		return std::max(kHeaderHeight, static_cast<int>(client.bottom) - kWaveformHeight);
	}

	int MotionPanel::CalculateVisibleRowCount() const {
		constexpr int curveRowUnits = kCurveGraphHeight / kRowHeight;
		int count = 0;
		for (const auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			if (!group.grouped) {
				for (const auto& row : group.rows)
					count += 1 + (row.expanded ? curveRowUnits : 0);
			} else {
				count++;
				if (group.expanded) {
					for (const auto& row : group.rows)
						count += 1 + (row.expanded ? curveRowUnits : 0);
				}
			}
		}
		return count;
	}

	void MotionPanel::DrawWaveform(const HDC deviceContext, const int top, const int right, const int bottom) const {
		const RECT waveformRect{0, top, right, bottom};
		GuiDrawer::FillRectColor(deviceContext, waveformRect, RGB(21, 25, 31));
		const RECT waveformLabelRect{0, top, kLabelWidth, bottom};
		GuiDrawer::FillRectColor(deviceContext, waveformLabelRect, RGB(40, 45, 54));
		GuiDrawer::DrawTextLine(deviceContext, Language::Text("panel.sound"),
			{8, top, kLabelWidth - 4, bottom}, RGB(228, 228, 232), DT_LEFT | DT_END_ELLIPSIS);
		for (int x = kLabelWidth; x < right; x += kFrameWidth) {
			const int frame = firstFrame + (x - kLabelWidth) / kFrameWidth;
			GuiDrawer::DrawLine(deviceContext, x, top, x, bottom, frame % 5 == 0 ? RGB(61, 72, 86) : RGB(39, 46, 56));
		}
		const int centerY = top + (bottom - top) / 2;
		if (waveform)
			GuiDrawer::DrawWaveform(deviceContext, {kLabelWidth, top, right, bottom},
				waveform->minimums, waveform->maximums, waveform->samplesPerFrame, firstFrame, kFrameWidth, RGB(92, 151, 255));
		GuiDrawer::DrawLine(deviceContext, kLabelWidth, centerY, right, centerY, RGB(96, 105, 120));
		GuiDrawer::DrawLine(deviceContext, 0, top, right, top, RGB(93, 98, 108));
		GuiDrawer::DrawLine(deviceContext, kLabelWidth, top, kLabelWidth, bottom, RGB(93, 98, 108));
	}

	void MotionPanel::Paint(const HDC deviceContext) const {
		RECT client{};
		GetClientRect(timelineWindow, &client);
		GuiDrawer::FillRectColor(deviceContext, client, RGB(26, 29, 35));
		const int timelineBottom = ResolveTimelineBottom();
		constexpr RECT modelHeader{0, 0, kLabelWidth, kHeaderHeight};
		GuiDrawer::FillRectColor(deviceContext, modelHeader, RGB(57, 61, 70));
		GuiDrawer::DrawTextLine(deviceContext, modelName.empty() ? Language::Text("motion.select_model") : modelName,
			{8, 0, kLabelWidth - 4, kHeaderHeight}, RGB(235, 235, 238), DT_LEFT | DT_END_ELLIPSIS);
		const int timelineWidth = client.right - kLabelWidth;
		const int visibleFrames = std::max(0, timelineWidth / kFrameWidth + 1);
		const int lastVisibleFrame = std::min(totalFrame, firstFrame + visibleFrames);
		for (int offset = 0; offset < visibleFrames; offset++) {
			const int frame = firstFrame + offset;
			const int x = kLabelWidth + offset * kFrameWidth;
			const bool major = frame % 5 == 0;
			GuiDrawer::DrawLine(deviceContext, x, 0, x, timelineBottom, major ? RGB(65, 77, 92) : RGB(43, 49, 59));
		}
		const int visibleRows = std::max(0, (timelineBottom - kHeaderHeight) / kRowHeight + 1);
		const int lastVisibleRow = firstRow + visibleRows;
		int visibleRowIndex = 0;
		const int rowClip = SaveDC(deviceContext);
		IntersectClipRect(deviceContext, 0, kHeaderHeight, client.right, timelineBottom);
		const auto DrawRow = [&](const std::wstring& name, const MotionTimelineGroup* group, const MotionTimelineRow* row,
			const bool groupRow, const bool expanded, const bool drawKeys, const int indent, const bool curveRow, const int rowUnits) {
			const int rowStart = visibleRowIndex;
			visibleRowIndex += rowUnits;
			if (rowStart + rowUnits <= firstRow || rowStart >= lastVisibleRow)
				return;
			const int top = kHeaderHeight + (rowStart - firstRow) * kRowHeight;
			const int rowHeight = rowUnits * kRowHeight;
			if (curveRow) {
				const RECT curveRect{0, top, client.right, top + rowHeight};
				GuiDrawer::FillRectColor(deviceContext, curveRect, RGB(31, 37, 45));
				for (int offset = 0; offset < visibleFrames; offset++) {
					const int frame = firstFrame + offset;
					const int x = kLabelWidth + offset * kFrameWidth;
					GuiDrawer::DrawLine(deviceContext, x, top, x, top + rowHeight, frame % 5 == 0 ? RGB(61, 72, 86) : RGB(39, 46, 56));
				}
			}
			const RECT labelRect{0, top, kLabelWidth, top + rowHeight};
			const COLORREF labelColor = curveRow ? RGB(37, 43, 52) : rowStart % 2 == 0 ? RGB(49, 53, 62) : RGB(43, 47, 55);
			GuiDrawer::FillRectColor(deviceContext, labelRect, labelColor);
			if (groupRow)
				GuiDrawer::DrawTriangle(deviceContext, 12, top + rowHeight / 2, 5, expanded, RGB(228, 228, 232));
			GuiDrawer::DrawTextLine(deviceContext, name, {indent, top, kLabelWidth - 4, top + rowHeight},
				RGB(228, 228, 232), DT_LEFT | DT_END_ELLIPSIS);
			GuiDrawer::DrawLine(deviceContext, 0, top + rowHeight, client.right, top + rowHeight, RGB(64, 67, 75));
			if (row && curveRow)
				DrawValueCurves(deviceContext, *row, top, top + rowHeight, client.right);
			if (!drawKeys)
				return;
			const auto DrawKey = [&](const int frame, const bool selected) {
				if (frame > totalFrame || frame < firstFrame)
					return;
				const int x = kLabelWidth + (frame - firstFrame) * kFrameWidth;
				if (x > client.right)
					return;
				GuiDrawer::DrawDiamond(deviceContext, x, top + rowHeight / 2, 5, selected ? RGB(246, 190, 53) : RGB(242, 242, 244));
			};
			if (curveRow)
				return;
			if (group) {
				auto frame = std::ranges::lower_bound(group->keyFrames, firstFrame);
				for (; frame != group->keyFrames.end() && *frame <= lastVisibleFrame; ++frame)
					DrawKey(*frame, IsGroupFrameSelected(*group, *frame));
			} else if (row) {
				auto key = std::ranges::lower_bound(row->keys, firstFrame, {}, &MotionTimelineKey::frame);
				for (; key != row->keys.end() && key->frame <= lastVisibleFrame; ++key)
					DrawKey(key->frame, key->selected);
			}
		};
		for (const auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			if (!group.grouped) {
				for (const auto& row : group.rows) {
					DrawRow(row.name, nullptr, &row, false, row.expanded, true, 32, false, 1);
					if (row.expanded)
						DrawRow(L"", nullptr, &row, false, false, false, 0, true, kCurveGraphHeight / kRowHeight);
				}
				continue;
			}
			DrawRow(group.name, &group, nullptr, true, group.expanded, !group.expanded, 24, false, 1);
			if (!group.expanded)
				continue;
			for (const auto& row : group.rows) {
				DrawRow(row.name, nullptr, &row, false, row.expanded, true, 32, false, 1);
				if (row.expanded)
					DrawRow(L"", nullptr, &row, false, false, false, 0, true, kCurveGraphHeight / kRowHeight);
			}
		}
		RestoreDC(deviceContext, rowClip);
		DrawWaveform(deviceContext, timelineBottom, client.right, client.bottom);
		if (selectingKeys) {
			const int left = std::min(selectionStart.x, selectionEnd.x);
			const int top = std::min(selectionStart.y, selectionEnd.y);
			const int right = std::max(selectionStart.x, selectionEnd.x);
			const int bottom = std::max(selectionStart.y, selectionEnd.y);
			GuiDrawer::DrawLine(deviceContext, left, top, right, top, RGB(100, 220, 255));
			GuiDrawer::DrawLine(deviceContext, right, top, right, bottom, RGB(100, 220, 255));
			GuiDrawer::DrawLine(deviceContext, right, bottom, left, bottom, RGB(100, 220, 255));
			GuiDrawer::DrawLine(deviceContext, left, bottom, left, top, RGB(100, 220, 255));
		}
		GuiDrawer::DrawLine(deviceContext, kLabelWidth, 0, kLabelWidth, client.bottom, RGB(93, 98, 108));
		if (currentFrame >= firstFrame) {
			const int currentX = kLabelWidth + (currentFrame - firstFrame) * kFrameWidth;
			if (currentX <= client.right)
				GuiDrawer::DrawLine(deviceContext, currentX, 0, currentX, client.bottom, RGB(52, 211, 235));
		}
		for (int offset = 0; offset < visibleFrames; offset++) {
			const int frame = firstFrame + offset;
			if (frame % 5 == 0) {
				const int x = kLabelWidth + offset * kFrameWidth;
				GuiDrawer::DrawTextLine(deviceContext, std::to_wstring(frame),
					{x + 2, 0, x + kFrameWidth * 5, kHeaderHeight}, RGB(207, 211, 218), DT_LEFT);
			}
		}
	}

	void MotionPanel::ToggleGroup(const int visibleRowIndex) {
		constexpr int curveRowUnits = kCurveGraphHeight / kRowHeight;
		int currentRow = 0;
		for (auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			if (!group.grouped) {
				for (auto& row : group.rows) {
					if (currentRow == visibleRowIndex && row.expandable) {
						row.expanded = !row.expanded;
						UpdateVerticalScrollBar();
						InvalidateRect(timelineWindow, nullptr, TRUE);
						return;
					}
					currentRow++;
					if (row.expanded)
						currentRow += curveRowUnits;
				}
				continue;
			}
			if (currentRow == visibleRowIndex) {
				group.expanded = !group.expanded;
				UpdateVerticalScrollBar();
				firstRow = GetScrollPos(timelineWindow, SB_VERT);
				InvalidateRect(timelineWindow, nullptr, TRUE);
				return;
			}
			currentRow++;
			if (!group.expanded)
				continue;
			for (auto& row : group.rows) {
				if (currentRow == visibleRowIndex && row.expandable) {
					row.expanded = !row.expanded;
					UpdateVerticalScrollBar();
					InvalidateRect(timelineWindow, nullptr, TRUE);
					return;
				}
				currentRow++;
				if (row.expanded)
					currentRow += curveRowUnits;
			}
		}
	}

	void MotionPanel::DrawValueCurves(
		const HDC deviceContext, const MotionTimelineRow& row, const int top, const int bottom, const int right) const {
		if (row.keys.empty())
			return;
		const int savedDc = SaveDC(deviceContext);
		IntersectClipRect(deviceContext, kLabelWidth, top, right, bottom);
		for (size_t channelIndex = 0; channelIndex < row.curveNames.size(); channelIndex++) {
			float minimum = std::numeric_limits<float>::max();
			float maximum = std::numeric_limits<float>::lowest();
			for (const auto& key : row.keys) {
				if (channelIndex >= key.values.size())
					continue;
				minimum = std::min(minimum, key.values[channelIndex]);
				maximum = std::max(maximum, key.values[channelIndex]);
			}
			if (minimum > maximum)
				continue;
			if (std::abs(maximum - minimum) < 1.0e-5f) {
				minimum -= 0.5f;
				maximum += 0.5f;
			}
			const auto ValueToY = [&](const float value) {
				return bottom - 8 - std::lround((value - minimum) / (maximum - minimum) * (bottom - top - 16));
			};
			for (size_t keyIndex = 1; keyIndex < row.keys.size(); keyIndex++) {
				const auto& previousKey = row.keys[keyIndex - 1];
				const auto& nextKey = row.keys[keyIndex];
				if (channelIndex >= previousKey.values.size() ||
					channelIndex >= nextKey.values.size() ||
					channelIndex >= nextKey.curves.size() ||
					nextKey.frame - previousKey.frame <= 1)
					continue;
				const auto& [p1, p2] = nextKey.curves[channelIndex];
				const int startX = kLabelWidth + (previousKey.frame - firstFrame) * kFrameWidth;
				const int endX = kLabelWidth + (nextKey.frame - firstFrame) * kFrameWidth;
				if (endX < kLabelWidth || startX > right)
					continue;
				const float startValue = previousKey.values[channelIndex];
				const float endValue = nextKey.values[channelIndex];
				const POINT points[] = {
					{startX, ValueToY(startValue)},
					{startX + std::lround((endX - startX) * p1.x), ValueToY(std::lerp(startValue, endValue, p1.y))},
					{startX + std::lround((endX - startX) * p2.x), ValueToY(std::lerp(startValue, endValue, p2.y))},
					{endX, ValueToY(endValue)}
				};
				const HPEN curvePen = CreatePen(PS_SOLID, 2, GuiTheme::ResolveCurveColor(channelIndex));
				const HGDIOBJ previousPen = SelectObject(deviceContext, curvePen);
				PolyBezier(deviceContext, points, 4);
				SelectObject(deviceContext, previousPen);
				DeleteObject(curvePen);
			}
			for (const auto& key : row.keys) {
				if (channelIndex >= key.values.size() || key.frame < firstFrame)
					continue;
				const int x = kLabelWidth + (key.frame - firstFrame) * kFrameWidth;
				if (x > right)
					break;
				const COLORREF keyColor = key.selected
					? GuiTheme::GetSelectedCurveKeyColor()
					: GuiTheme::ResolveCurveColor(channelIndex);
				GuiDrawer::DrawDiamond(deviceContext, x, ValueToY(key.values[channelIndex]), 5, keyColor);
			}
		}
		RestoreDC(deviceContext, savedDc);
		const int legendCount = std::min(static_cast<int>(row.curveNames.size()), 6);
		for (int index = 0; index < legendCount; index++) {
			const int y = top + 12 + index * 22;
			GuiDrawer::DrawDiamond(deviceContext, 18, y, 5, GuiTheme::ResolveCurveColor(index));
			GuiDrawer::DrawTextLine(deviceContext, row.curveNames[index],
				{30, y - 10, kLabelWidth - 6, y + 10}, RGB(228, 228, 232), DT_LEFT | DT_END_ELLIPSIS);
		}
	}

	bool MotionPanel::SelectKey(const int visibleRowIndex, const int x, const bool additive) {
		constexpr int curveRowUnits = kCurveGraphHeight / kRowHeight;
		const auto SelectRowKey = [&](MotionTimelineRow& row) {
			for (auto& key : row.keys) {
				const int keyX = kLabelWidth + (key.frame - firstFrame) * kFrameWidth;
				if (std::abs(keyX - x) > 6)
					continue;
				const bool select = !additive || !key.selected;
				if (!additive)
					ClearKeySelection();
				key.selected = select;
				interpolationSelectionDirty = true;
				InvalidateRect(timelineWindow, nullptr, FALSE);
				return true;
			}
			return false;
		};
		int currentRow = 0;
		for (auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			if (!group.grouped) {
				for (auto& row : group.rows) {
					const int rowEnd = currentRow + (row.expanded ? curveRowUnits : 0);
					if (visibleRowIndex >= currentRow && visibleRowIndex <= rowEnd)
						return SelectRowKey(row);
					currentRow = rowEnd + 1;
				}
				continue;
			}
			if (currentRow == visibleRowIndex && !group.expanded) {
				for (const int frame : group.keyFrames) {
					const int keyX = kLabelWidth + (frame - firstFrame) * kFrameWidth;
					if (std::abs(keyX - x) > 6)
						continue;
					const bool select = !additive || !IsGroupFrameSelected(group, frame);
					if (!additive)
						ClearKeySelection();
					for (auto& row : group.rows) {
						auto& keys = row.keys;
						for (auto& key : keys) {
							if (key.frame == frame)
								key.selected = select;
						}
					}
					interpolationSelectionDirty = true;
					InvalidateRect(timelineWindow, nullptr, FALSE);
					return true;
				}
				return false;
			}
			currentRow++;
			if (!group.expanded)
				continue;
			for (auto& row : group.rows) {
				const int rowEnd = currentRow + (row.expanded ? curveRowUnits : 0);
				if (visibleRowIndex >= currentRow && visibleRowIndex <= rowEnd)
					return SelectRowKey(row);
				currentRow = rowEnd + 1;
			}
		}
		return false;
	}

	void MotionPanel::SelectKeysInRectangle(const bool additive) {
		constexpr int curveRowUnits = kCurveGraphHeight / kRowHeight;
		const RECT selectionRect{
			std::min(selectionStart.x, selectionEnd.x),
			std::min(selectionStart.y, selectionEnd.y),
			std::max(selectionStart.x, selectionEnd.x),
			std::max(selectionStart.y, selectionEnd.y)
		};
		if (!additive)
			ClearKeySelection();
		int visibleRow = 0;
		const auto SelectRowKeys = [&](MotionTimelineRow& row, const int rowIndex) {
			const int rowY = kHeaderHeight + (rowIndex - firstRow) * kRowHeight + kRowHeight / 2;
			for (auto& key : row.keys) {
				const int x = kLabelWidth + (key.frame - firstFrame) * kFrameWidth;
				if (x >= selectionRect.left && x <= selectionRect.right
					&& rowY >= selectionRect.top && rowY <= selectionRect.bottom)
					key.selected = true;
			}
		};
		const auto SelectCurveKeys = [&](MotionTimelineRow& row, const int rowIndex) {
			const int top = kHeaderHeight + (rowIndex - firstRow) * kRowHeight;
			const int bottom = top + kCurveGraphHeight;
			for (size_t channelIndex = 0; channelIndex < row.curveNames.size(); channelIndex++) {
				float minimum = std::numeric_limits<float>::max();
				float maximum = std::numeric_limits<float>::lowest();
				for (const auto& key : row.keys) {
					if (channelIndex >= key.values.size())
						continue;
					minimum = std::min(minimum, key.values[channelIndex]);
					maximum = std::max(maximum, key.values[channelIndex]);
				}
				if (minimum > maximum)
					continue;
				if (std::abs(maximum - minimum) < 1.0e-5f) {
					minimum -= 0.5f;
					maximum += 0.5f;
				}
				const auto ValueToY = [&](const float value) {
					return bottom - 8 - std::lround((value - minimum) / (maximum - minimum) * (bottom - top - 16));
				};
				for (auto& key : row.keys) {
					if (channelIndex >= key.values.size())
						continue;
					const int x = kLabelWidth + (key.frame - firstFrame) * kFrameWidth;
					const int y = ValueToY(key.values[channelIndex]);
					if (x >= selectionRect.left && x <= selectionRect.right
						&& y >= selectionRect.top && y <= selectionRect.bottom)
						key.selected = true;
				}
			}
		};
		for (auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			if (!group.grouped) {
				for (auto& row : group.rows) {
					SelectRowKeys(row, visibleRow);
					visibleRow++;
					if (row.expanded) {
						SelectCurveKeys(row, visibleRow);
						visibleRow += curveRowUnits;
					}
				}
				continue;
			}
			const int groupY = kHeaderHeight + (visibleRow - firstRow) * kRowHeight + kRowHeight / 2;
			if (!group.expanded) {
				for (const int frame : group.keyFrames) {
					const int x = kLabelWidth + (frame - firstFrame) * kFrameWidth;
					if (x < selectionRect.left || x > selectionRect.right
						|| groupY < selectionRect.top || groupY > selectionRect.bottom)
						continue;
					for (auto& row : group.rows) {
						auto& keys = row.keys;
						for (auto& key : keys) {
							if (key.frame == frame)
								key.selected = true;
						}
					}
				}
			}
			visibleRow++;
			if (!group.expanded)
				continue;
			for (auto& row : group.rows) {
				SelectRowKeys(row, visibleRow);
				visibleRow++;
				if (row.expanded) {
					SelectCurveKeys(row, visibleRow);
					visibleRow += curveRowUnits;
				}
			}
		}
		interpolationSelectionDirty = true;
	}

	void MotionPanel::ClearKeySelection() {
		for (auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			for (auto& row : group.rows) {
				for (auto& key : row.keys)
					key.selected = false;
			}
		}
	}

	bool MotionPanel::IsGroupFrameSelected(const MotionTimelineGroup& group, const int frame) {
		for (const auto& row : group.rows) {
			const auto key = std::ranges::lower_bound(row.keys, frame, {}, &MotionTimelineKey::frame);
			if (key != row.keys.end() && key->frame == frame && key->selected)
				return true;
		}
		return false;
	}

	InterpolationSelection MotionPanel::BuildInterpolationSelection() const {
		static constexpr size_t kMaxCurvesPerChannel = 256;
		InterpolationSelection selection;
		for (const auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			for (const auto& row : group.rows) {
				for (const auto& key : row.keys) {
					if (!key.selected)
						continue;
					selection.selectedKeyCount++;
					if (selection.channels.size() < key.curves.size()) {
						const size_t previousSize = selection.channels.size();
						selection.channels.resize(key.curves.size());
						for (size_t index = previousSize; index < selection.channels.size(); index++) {
							if (index < row.curveNames.size())
								selection.channels[index].name = row.curveNames[index];
						}
					}
					for (size_t index = 0; index < key.curves.size(); index++) {
						auto& curves = selection.channels[index].curves;
						if (curves.size() < kMaxCurvesPerChannel)
							curves.emplace_back(key.curves[index]);
					}
				}
			}
		}
		return selection;
	}

	void MotionPanel::ToggleMode() {
		ChangeMode(mode == MotionTimelineMode::Model ? MotionTimelineMode::Camera : MotionTimelineMode::Model);
	}

	void MotionPanel::UpdateModeButtonText() const {
		if (!modeButton)
			return;
		SetWindowTextW(modeButton, Language::Text(mode == MotionTimelineMode::Model ? "motion.camera" : "motion.model").c_str());
	}

	void MotionPanel::ScrollRows(const int scrollCode, const int trackPosition) {
		SCROLLINFO info{};
		info.cbSize = sizeof(info);
		info.fMask = SIF_ALL;
		GetScrollInfo(timelineWindow, SB_VERT, &info);
		int position = info.nPos;
		switch (scrollCode) {
			case SB_LINEUP: position--; break;
			case SB_LINEDOWN: position++; break;
			case SB_PAGEUP: position -= info.nPage; break;
			case SB_PAGEDOWN: position += info.nPage; break;
			case SB_THUMBPOSITION:
			case SB_THUMBTRACK: position = trackPosition; break;
			default: return;
		}
		firstRow = std::clamp(position, info.nMin, std::max(info.nMin, info.nMax - static_cast<int>(info.nPage) + 1));
		SetScrollPos(timelineWindow, SB_VERT, firstRow, TRUE);
		InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	void MotionPanel::ScrollFrames(const int scrollCode, const int) {
		SCROLLINFO info{};
		info.cbSize = sizeof(info);
		info.fMask = SIF_ALL;
		GetScrollInfo(timelineWindow, SB_HORZ, &info);
		int position = playing ? firstFrame : currentFrame;
		RECT client{};
		GetClientRect(timelineWindow, &client);
		const int timelineWidth = client.right - kLabelWidth;
		const int visibleFrames = std::max(1, timelineWidth / kFrameWidth);
		switch (scrollCode) {
			case SB_LINELEFT: position--; break;
			case SB_LINERIGHT: position++; break;
			case SB_PAGELEFT: position -= visibleFrames; break;
			case SB_PAGERIGHT: position += visibleFrames; break;
			case SB_THUMBPOSITION:
			case SB_THUMBTRACK: position = info.nTrackPos; break;
			case SB_LEFT: position = 0; break;
			case SB_RIGHT: position = totalFrame; break;
			case SB_ENDSCROLL:
				if (playing)
					return;
				seekRequested = true;
				seekFinished = true;
				return;
			default: return;
		}
		if (playing) {
			manualFrameScroll = true;
			firstFrame = std::clamp(position, 0, totalFrame);
			SetScrollPos(timelineWindow, SB_HORZ, firstFrame, TRUE);
			InvalidateRect(timelineWindow, nullptr, FALSE);
			return;
		}
		currentFrame = std::clamp(position, 0, totalFrame);
		seekFrame = currentFrame;
		seekRequested = true;
		seekFinished = scrollCode != SB_THUMBTRACK;
		SetScrollPos(timelineWindow, SB_HORZ, currentFrame, TRUE);
		UpdateFrameEditText(true);
		FollowCurrentFrame();
		InvalidateRect(timelineWindow, nullptr, FALSE);
	}

	void MotionPanel::FollowCurrentFrame() {
		RECT client{};
		GetClientRect(timelineWindow, &client);
		const int timelineWidth = std::max(0, static_cast<int>(client.right) - kLabelWidth);
		const int visibleFrames = std::max(1, timelineWidth / kFrameWidth);
		const int current = currentFrame;
		const int nextFirstFrame = std::max(0, current - visibleFrames / 2);
		if (firstFrame == nextFirstFrame)
			return;
		firstFrame = nextFirstFrame;
	}

	void MotionPanel::ApplyPlaybackState(const bool isPlaying) {
		if (!isPlaying)
			manualFrameScroll = false;
		playing = isPlaying;
		if (frameEdit)
			EnableWindow(frameEdit, isPlaying ? FALSE : TRUE);
	}

	void MotionPanel::AttachWaveform(const AudioWaveform& audioWaveform) {
		waveform = &audioWaveform;
		if (timelineWindow)
			InvalidateRect(timelineWindow, nullptr, FALSE);
	}

	void MotionPanel::ChangeMode(const MotionTimelineMode timelineMode) {
		if (mode == timelineMode)
			return;
		mode = timelineMode;
		firstRow = 0;
		manualFrameScroll = false;
		interpolationSelectionDirty = true;
		UpdateModeButtonText();
		UpdateVerticalScrollBar();
		if (timelineWindow)
			InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	void MotionPanel::Create(const HWND parent) {
		if (timelineWindow)
			return;
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
		windowClass.lpszClassName = L"PmxModMotionTimeline";
		RegisterClassExW(&windowClass);
		timelineWindow = CreateWindowExW(
			WS_EX_CLIENTEDGE, windowClass.lpszClassName, L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
			0, 0, 0, 0,
			parent, nullptr, instance, this);
		frameEdit = CreateWindowExW(
			WS_EX_CLIENTEDGE, L"EDIT", L"0",
			WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER | ES_AUTOHSCROLL,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(kFrameEditId), instance, nullptr);
		modeButton = CreateWindowExW(
			0, L"BUTTON", L"",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0, parent,
			reinterpret_cast<HMENU>(kModeButtonId),
			instance,
			nullptr);
		SendMessageW(frameEdit, EM_SETLIMITTEXT, 5, 0);
		SetWindowSubclass(frameEdit, EditWindowProc, kFrameEditId, reinterpret_cast<DWORD_PTR>(this));
		GuiTheme::ApplyControl(frameEdit);
		GuiTheme::ApplyControl(modeButton);
		UpdateModeButtonText();
		ApplyPlaybackState(playing);
		UpdateVerticalScrollBar();
		UpdateHorizontalScrollBar();
	}

	void MotionPanel::Resize(const RECT& clientRect) {
		if (!timelineWindow)
			return;
		constexpr int margin = 8;
		constexpr int toolbarHeight = 32;
		constexpr int editWidth = 72;
		constexpr int editHeight = 22;
		constexpr int buttonWidth = 84;
		const int x = clientRect.left + margin;
		const int y = clientRect.top + margin + toolbarHeight;
		const int width = std::max(0, static_cast<int>(clientRect.right - clientRect.left) - margin * 2);
		const int height = std::max(0, static_cast<int>(clientRect.bottom - clientRect.top) - margin * 2 - toolbarHeight);
		const int editX = clientRect.right - margin - editWidth;
		MoveWindow(modeButton, clientRect.left + margin, clientRect.top + margin, buttonWidth, editHeight, TRUE);
		MoveWindow(frameEdit, editX, clientRect.top + margin, editWidth, editHeight, TRUE);
		MoveWindow(timelineWindow, x, y, width, height, TRUE);
		UpdateVerticalScrollBar();
		UpdateHorizontalScrollBar();
	}

	void MotionPanel::UpdateLanguage() {
		UpdateModeButtonText();
		if (timelineWindow)
			InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	bool MotionPanel::HandleCommand(const UINT_PTR commandId, const int notificationCode) {
		if (commandId == kModeButtonId && notificationCode == BN_CLICKED) {
			ToggleMode();
			return true;
		}
		if (commandId != kFrameEditId)
			return false;
		if (notificationCode == EN_CHANGE)
			ApplyInputFrame();
		else if (notificationCode == EN_KILLFOCUS)
			ClampInputFrameToScrollRange();
		return true;
	}

	void MotionPanel::Destroy() {
		if (modeButton)
			DestroyWindow(modeButton);
		if (frameEdit)
			DestroyWindow(frameEdit);
		if (timelineWindow)
			DestroyWindow(timelineWindow);
		timelineWindow = nullptr;
		frameEdit = nullptr;
		modeButton = nullptr;
		modelName.clear();
		groups.clear();
		totalFrame = 0;
		currentFrame = 0;
		firstRow = 0;
		firstFrame = 0;
		seekRequested = false;
		seekFinished = false;
		interpolationSelectionDirty = false;
		selectingKeys = false;
		playing = false;
		manualFrameScroll = false;
		mode = MotionTimelineMode::Camera;
		seekFrame = 0;
	}

	void MotionPanel::ApplyTimeline(std::wstring name, std::vector<MotionTimelineGroup> timelineGroups) {
		modelName = std::move(name);
		groups = std::move(timelineGroups);
		firstRow = 0;
		interpolationSelectionDirty = true;
		FollowCurrentFrame();
		UpdateVerticalScrollBar();
		UpdateHorizontalScrollBar();
		if (timelineWindow)
			InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	void MotionPanel::UpdateLastFrame(const int maxFrame) {
		totalFrame = std::clamp(maxFrame, 0, kMaxEditableFrame);
		UpdateHorizontalScrollBar();
		if (timelineWindow)
			InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	void MotionPanel::UpdateCurrentFrame(const int frame) {
		const int timelineFrame = std::clamp(frame, 0, kMaxEditableFrame);
		if (currentFrame == timelineFrame)
			return;
		currentFrame = timelineFrame;
		if (timelineWindow) {
			if (!playing || !manualFrameScroll)
				FollowCurrentFrame();
			SetScrollPos(timelineWindow, SB_HORZ, playing && manualFrameScroll ? firstFrame : std::min(currentFrame, totalFrame), TRUE);
			UpdateFrameEditText();
			InvalidateRect(timelineWindow, nullptr, FALSE);
		}
	}

	bool MotionPanel::ConsumeSeekFrame(int& frame, bool& finished) {
		if (!seekRequested)
			return false;
		frame = seekFrame;
		finished = seekFinished;
		seekRequested = false;
		seekFinished = false;
		return true;
	}

	bool MotionPanel::ConsumeInterpolationSelection(InterpolationSelection& selection) {
		if (!interpolationSelectionDirty)
			return false;
		selection = BuildInterpolationSelection();
		interpolationSelectionDirty = false;
		return true;
	}
}
