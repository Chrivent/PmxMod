#pragma once

#include "Panel/Panel.h"

#include <string>
#include <vector>

namespace Chrivent {
	class MenuBar;

	enum class PanelWindowArea {
		Model,
		Motion,
		InterpolationCurve,
		Bottom
	};

	class PanelWindow {
		struct PanelEntry {
			Panel* panel = nullptr;
			std::string titleKey;
			PanelWindowArea area = PanelWindowArea::Bottom;
			HWND frame = nullptr;
		};

		HWND window = nullptr;
		MenuBar* menuBar = nullptr;
		std::vector<PanelEntry> panels;
		bool closeRequested = false;

		// 패널 창의 Win32 메시지를 처리한다.
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		// 등록된 패널의 프레임과 내부 컨트롤을 생성한다.
		void CreatePanelControls();
		// 패널 창 크기에 맞춰 등록된 패널 영역을 배치한다.
		void LayoutPanels() const;

	public:
		PanelWindow() = default;
		~PanelWindow();

		bool IsCloseRequested() const { return closeRequested; }

		// 패널 창에서 사용할 메뉴바를 연결한다.
		void AttachMenuBar(MenuBar& menu);
		// 패널과 제목, 배치 영역을 패널 창에 등록한다.
		void RegisterPanel(Panel& panel, std::string titleKey, PanelWindowArea area);
		// 패널 창과 등록된 패널 컨트롤을 생성해 표시한다.
		void Show();
		// 패널 창에 쌓인 Win32 메시지를 처리한다.
		void Poll() const;
		// 현재 언어에 맞춰 창, 메뉴, 패널 제목과 컨트롤 문구를 갱신한다.
		void RefreshLanguage() const;
		// 패널 창과 등록된 패널 컨트롤을 해제한다.
		void Destroy();
	};
}
