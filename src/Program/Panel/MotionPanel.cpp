#include "MotionPanel.h"

#include "../GuiBackBuffer.h"
#include "../GuiDrawer.h"
#include "../Language.h"

#include <CommCtrl.h>
#include <algorithm>
#include <cstdlib>
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
				if (y >= kHeaderHeight) {
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
						const int visibleRow = panel->firstRow
							+ (panel->selectionStart.y - kHeaderHeight) / kRowHeight;
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
		const HWND hwnd,
		const UINT msg,
		const WPARAM wParam,
		const LPARAM lParam,
		const UINT_PTR subclassId,
		const DWORD_PTR data) {
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
		SetScrollPos(timelineWindow, SB_HORZ, (std::min)(currentFrame, totalFrame), TRUE);
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

	void MotionPanel::UpdateFrameEditText() {
		if (!frameEdit || GetFocus() == frameEdit)
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
		const int visibleRows = (std::max)(1, static_cast<int>((client.bottom - kHeaderHeight) / kRowHeight));
		SCROLLINFO vertical{};
		vertical.cbSize = sizeof(vertical);
		vertical.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		vertical.nMin = 0;
		vertical.nMax = (std::max)(0, GetVisibleRowCount() - 1);
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
		horizontal.nPos = (std::min)(currentFrame, totalFrame);
		SetScrollInfo(timelineWindow, SB_HORZ, &horizontal, TRUE);
		ShowScrollBar(timelineWindow, SB_HORZ, TRUE);
	}

	int MotionPanel::GetVisibleRowCount() const {
		int count = 0;
		for (const auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			if (!group.grouped)
				count += group.rows.size();
			else {
				count++;
				if (group.expanded)
					count += group.rows.size();
			}
		}
		return count;
	}

	void MotionPanel::Paint(const HDC deviceContext) const {
		RECT client{};
		GetClientRect(timelineWindow, &client);
		GuiDrawer::FillRectColor(deviceContext, client, RGB(26, 29, 35));
		constexpr RECT modelHeader{0, 0, kLabelWidth, kHeaderHeight};
		GuiDrawer::FillRectColor(deviceContext, modelHeader, RGB(57, 61, 70));
		GuiDrawer::DrawTextLine(deviceContext, modelName.empty() ? Language::Text("motion.select_model") : modelName,
			{8, 0, kLabelWidth - 4, kHeaderHeight}, RGB(235, 235, 238), DT_LEFT | DT_END_ELLIPSIS);
		const int timelineWidth = client.right - kLabelWidth;
		const int visibleFrames = (std::max)(0, timelineWidth / kFrameWidth + 1);
		const uint32_t lastVisibleFrame = (std::min)(totalFrame, static_cast<uint32_t>(firstFrame + visibleFrames));
		for (int offset = 0; offset < visibleFrames; offset++) {
			const int frame = firstFrame + offset;
			const int x = kLabelWidth + offset * kFrameWidth;
			const bool major = frame % 5 == 0;
			GuiDrawer::DrawLine(deviceContext, x, 0, x, client.bottom,
				major ? RGB(65, 77, 92) : RGB(43, 49, 59));
		}
		const int visibleRows = (std::max)(0, static_cast<int>((client.bottom - kHeaderHeight) / kRowHeight + 1));
		int visibleRowIndex = 0;
		int paintedRows = 0;
		const auto DrawRow = [&](
			const std::wstring& name,
			const MotionTimelineGroup* group,
			const MotionTimelineRow* row,
			const bool groupRow,
			const bool expanded,
			const bool drawKeys) {
			if (visibleRowIndex++ < firstRow || paintedRows >= visibleRows)
				return;
			const int top = kHeaderHeight + paintedRows * kRowHeight;
			const RECT labelRect{0, top, kLabelWidth, top + kRowHeight};
			GuiDrawer::FillRectColor(deviceContext, labelRect,
				paintedRows % 2 == 0 ? RGB(49, 53, 62) : RGB(43, 47, 55));
			if (groupRow)
				GuiDrawer::DrawTriangle(deviceContext, 12, top + kRowHeight / 2, 5, expanded, RGB(228, 228, 232));
			GuiDrawer::DrawTextLine(deviceContext, name,
				{groupRow ? 24 : 32, top, kLabelWidth - 4, top + kRowHeight},
				RGB(228, 228, 232), DT_LEFT | DT_END_ELLIPSIS);
			GuiDrawer::DrawLine(deviceContext, 0, top + kRowHeight, client.right, top + kRowHeight, RGB(64, 67, 75));
			if (!drawKeys) {
				paintedRows++;
				return;
			}
			const auto DrawKey = [&](const uint32_t frame, const bool selected) {
				if (frame > totalFrame || frame < firstFrame)
					return;
				const int x = kLabelWidth + (frame - firstFrame) * kFrameWidth;
				if (x > client.right)
					return;
				GuiDrawer::DrawDiamond(deviceContext, x, top + kRowHeight / 2, 5, selected ? RGB(246, 190, 53) : RGB(242, 242, 244));
			};
			if (group) {
				auto frame = std::ranges::lower_bound(group->keyFrames, static_cast<uint32_t>(firstFrame));
				for (; frame != group->keyFrames.end() && *frame <= lastVisibleFrame; ++frame)
					DrawKey(*frame, IsGroupFrameSelected(*group, *frame));
			} else if (row) {
				auto key = std::ranges::lower_bound(row->keys, static_cast<uint32_t>(firstFrame), {}, &MotionTimelineKey::frame);
				for (; key != row->keys.end() && key->frame <= lastVisibleFrame; ++key)
					DrawKey(key->frame, key->selected);
			}
			paintedRows++;
		};
		for (const auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			if (!group.grouped) {
				for (const auto& row : group.rows)
					DrawRow(row.name, nullptr, &row, false, false, true);
				continue;
			}
			DrawRow(group.name, &group, nullptr, true, group.expanded, !group.expanded);
			if (paintedRows >= visibleRows)
				break;
			if (!group.expanded)
				continue;
			for (const auto& row : group.rows) {
				DrawRow(row.name, nullptr, &row, false, false, true);
				if (paintedRows >= visibleRows)
					break;
			}
		}
		if (selectingKeys) {
			const int left = (std::min)(selectionStart.x, selectionEnd.x);
			const int top = (std::min)(selectionStart.y, selectionEnd.y);
			const int right = (std::max)(selectionStart.x, selectionEnd.x);
			const int bottom = (std::max)(selectionStart.y, selectionEnd.y);
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
		int currentRow = 0;
		for (auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			if (!group.grouped) {
				currentRow += group.rows.size();
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
			if (group.expanded)
				currentRow += group.rows.size();
		}
	}

	bool MotionPanel::SelectKey(const int visibleRowIndex, const int x, const bool additive) {
		int currentRow = 0;
		for (auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			if (!group.grouped) {
				for (auto& row : group.rows) {
					auto& keys = row.keys;
					if (currentRow++ != visibleRowIndex)
						continue;
					for (auto& key : keys) {
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
				}
				continue;
			}
			if (currentRow == visibleRowIndex && !group.expanded) {
				for (const uint32_t frame : group.keyFrames) {
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
				auto& keys = row.keys;
				if (currentRow++ != visibleRowIndex)
					continue;
				for (auto& key : keys) {
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
			}
		}
		return false;
	}

	void MotionPanel::SelectKeysInRectangle(const bool additive) {
		const RECT selectionRect{
			(std::min)(selectionStart.x, selectionEnd.x),
			(std::min)(selectionStart.y, selectionEnd.y),
			(std::max)(selectionStart.x, selectionEnd.x),
			(std::max)(selectionStart.y, selectionEnd.y)
		};
		if (!additive)
			ClearKeySelection();
		int visibleRow = 0;
		for (auto& group : groups) {
			if (!IsGroupVisible(group))
				continue;
			if (!group.grouped) {
				for (auto& row : group.rows) {
					auto& keys = row.keys;
					const int rowY = kHeaderHeight + (visibleRow - firstRow) * kRowHeight + kRowHeight / 2;
					for (auto& key : keys) {
						const int x = kLabelWidth + (key.frame - firstFrame) * kFrameWidth;
						if (x >= selectionRect.left && x <= selectionRect.right
							&& rowY >= selectionRect.top && rowY <= selectionRect.bottom)
							key.selected = true;
					}
					visibleRow++;
				}
				continue;
			}
			const int groupY = kHeaderHeight + (visibleRow - firstRow) * kRowHeight + kRowHeight / 2;
			if (!group.expanded) {
				for (const uint32_t frame : group.keyFrames) {
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
				const int rowY = kHeaderHeight + (visibleRow - firstRow) * kRowHeight + kRowHeight / 2;
				for (auto& key : row.keys) {
					const int x = kLabelWidth + (key.frame - firstFrame) * kFrameWidth;
					if (x >= selectionRect.left && x <= selectionRect.right
						&& rowY >= selectionRect.top && rowY <= selectionRect.bottom)
						key.selected = true;
				}
				visibleRow++;
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

	bool MotionPanel::IsGroupVisible(const MotionTimelineGroup& group) const {
		return group.mode == mode;
	}

	bool MotionPanel::IsGroupFrameSelected(const MotionTimelineGroup& group, const uint32_t frame) {
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
		mode = mode == MotionTimelineMode::Model
			? MotionTimelineMode::Camera
			: MotionTimelineMode::Model;
		firstRow = 0;
		interpolationSelectionDirty = true;
		UpdateModeButtonText();
		UpdateVerticalScrollBar();
		if (timelineWindow)
			InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	void MotionPanel::UpdateModeButtonText() const {
		if (!modeButton)
			return;
		SetWindowTextW(
			modeButton,
			Language::Text(mode == MotionTimelineMode::Model ? "motion.camera" : "motion.model").c_str());
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
		firstRow = std::clamp(position, info.nMin,
			(std::max)(info.nMin, info.nMax - static_cast<int>(info.nPage) + 1));
		SetScrollPos(timelineWindow, SB_VERT, firstRow, TRUE);
		InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	void MotionPanel::ScrollFrames(const int scrollCode, const int) {
		SCROLLINFO info{};
		info.cbSize = sizeof(info);
		info.fMask = SIF_ALL;
		GetScrollInfo(timelineWindow, SB_HORZ, &info);
		int position = currentFrame;
		RECT client{};
		GetClientRect(timelineWindow, &client);
		const int timelineWidth = client.right - kLabelWidth;
		const int visibleFrames = (std::max)(1, timelineWidth / kFrameWidth);
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
				seekRequested = true;
				seekFinished = true;
				return;
			default: return;
		}
		currentFrame = std::clamp(position, 0, static_cast<int>(totalFrame));
		seekFrame = currentFrame;
		seekRequested = true;
		seekFinished = scrollCode != SB_THUMBTRACK;
		SetScrollPos(timelineWindow, SB_HORZ, currentFrame, TRUE);
		FollowCurrentFrame();
		InvalidateRect(timelineWindow, nullptr, FALSE);
	}

	void MotionPanel::FollowCurrentFrame() {
		RECT client{};
		GetClientRect(timelineWindow, &client);
		const int timelineWidth = (std::max)(0, static_cast<int>(client.right) - kLabelWidth);
		const int visibleFrames = (std::max)(1, timelineWidth / kFrameWidth);
		const int current = currentFrame;
		const int nextFirstFrame = (std::max)(0, current - visibleFrames / 2);
		if (firstFrame == nextFirstFrame)
			return;
		firstFrame = nextFirstFrame;
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
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFrameEditId)), instance, nullptr);
		modeButton = CreateWindowExW(
			0, L"BUTTON", L"",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0, parent,
			reinterpret_cast<HMENU>(static_cast<INT_PTR>(kModeButtonId)),
			instance,
			nullptr);
		SendMessageW(frameEdit, EM_SETLIMITTEXT, 5, 0);
		SetWindowSubclass(frameEdit, EditWindowProc, kFrameEditId, reinterpret_cast<DWORD_PTR>(this));
		UpdateModeButtonText();
		UpdateVerticalScrollBar();
		UpdateHorizontalScrollBar();
	}

	void MotionPanel::Resize(const RECT& clientRect) {
		if (!timelineWindow)
			return;
		constexpr int margin = 8;
		constexpr int toolbarHeight = 28;
		constexpr int editWidth = 72;
		constexpr int editHeight = 22;
		constexpr int buttonWidth = 84;
		const int x = clientRect.left + margin;
		const int y = clientRect.top + margin + toolbarHeight;
		const int width = (std::max)(0, static_cast<int>(clientRect.right - clientRect.left) - margin * 2);
		const int height = (std::max)(0, static_cast<int>(clientRect.bottom - clientRect.top) - margin * 2 - toolbarHeight);
		const int editX = clientRect.right - margin - editWidth;
		MoveWindow(modeButton, clientRect.left + margin, clientRect.top + margin,
			buttonWidth, editHeight, TRUE);
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

	bool MotionPanel::HandleCommand(const int commandId, const int notificationCode) {
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
		mode = MotionTimelineMode::Model;
		seekFrame = 0;
	}

	void MotionPanel::SetTimeline(std::wstring name, std::vector<MotionTimelineGroup> timelineGroups) {
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

	void MotionPanel::SetLastFrame(const uint32_t maxFrame) {
		totalFrame = maxFrame;
		UpdateHorizontalScrollBar();
		if (timelineWindow)
			InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	void MotionPanel::SetCurrentFrame(const uint32_t frame) {
		const uint32_t timelineFrame = (std::min)(frame, static_cast<uint32_t>(kMaxEditableFrame));
		if (currentFrame == timelineFrame)
			return;
		currentFrame = timelineFrame;
		if (timelineWindow) {
			FollowCurrentFrame();
			SetScrollPos(timelineWindow, SB_HORZ, (std::min)(currentFrame, totalFrame), TRUE);
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
