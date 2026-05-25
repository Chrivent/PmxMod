#pragma once

#include "Panel/Panel.h"

#include <string>
#include <vector>

namespace Chrivent {
	class MenuBar;

	enum class PanelWindowArea {
		Model,
		Motion,
		Playback,
		Bottom
	};

	class PanelWindow {
		struct PanelEntry {
			Panel* panel = nullptr;
			std::wstring title;
			PanelWindowArea area = PanelWindowArea::Bottom;
			HWND frame = nullptr;
		};

		HWND window = nullptr;
		MenuBar* menuBar = nullptr;
		std::vector<PanelEntry> panels;

		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		void CreatePanelControls();
		void LayoutPanels() const;

	public:
		PanelWindow() = default;
		~PanelWindow();
		
		void AttachMenuBar(MenuBar& menu);
		void RegisterPanel(Panel& panel, const std::wstring& title, PanelWindowArea area);
		void Show();
		void Poll() const;
		void Destroy();
	};
}
