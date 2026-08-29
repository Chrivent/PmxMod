#pragma once

#include "Program/Panel/Panel.h"
#include "Program/Settings.h"

#include <functional>
#include <string>
#include <vector>

namespace Chrivent {
	class MenuBar;

	enum class PanelWindowArea {
		Model,
		Information,
		Motion,
		InterpolationCurve,
		Bottom
	};

	// 여러 패널을 담는 Win32 창의 생성, 배치와 메시지 처리를 담당한다.
	class PanelWindow {
		enum class DragBoundary {
			None,
			Left,
			Right,
			Bottom
		};

		// 패널 객체와 창 내부 배치 정보를 함께 보관한다.
		struct PanelEntry {
			Panel* panel = nullptr;
			std::string titleKey;
			PanelWindowArea area = PanelWindowArea::Bottom;
			HWND frame = nullptr;
			RECT bounds{};
			bool visible = true;
		};

		HWND window = nullptr;
		MenuBar* menuBar = nullptr;
		std::function<void()> interactionFinishedCallback;
		std::function<bool()> menuFrameCallback;
		std::vector<PanelEntry> panels;
		bool closeRequested = false;
		PanelLayoutSettings layoutSettings;
		DragBoundary dragBoundary = DragBoundary::None;

		static constexpr UINT_PTR kMenuFrameTimerId = 9001;

		// 패널 창의 Win32 메시지를 처리한다.
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		// 메뉴바 모달 루프 중 렌더링을 유지하는 타이머를 켠다.
		void StartMenuFrameTimer() const;
		// 메뉴바 모달 루프에서 빠져나오면 렌더링 타이머를 끈다.
		void StopMenuFrameTimer() const;
		// 메뉴바 조작 중 렌더링 프레임을 한 번 요청한다.
		bool RenderMenuFrame() const;
		// 창 이동이나 패널 경계 드래그가 끝났음을 외부에 알린다.
		void NotifyInteractionFinished() const;
		// 등록된 패널의 프레임과 내부 컨트롤을 생성한다.
		void CreatePanelControls();
		// 패널 영역의 배경과 경계선을 그린다.
		void Paint(HDC deviceContext) const;
		// 패널 창 크기에 맞춰 등록된 패널 영역을 배치한다.
		void LayoutPanels();
		// 마우스 좌표에 있는 조절 가능한 패널 경계를 반환한다.
		DragBoundary HitTestBoundary(int x, int y) const;
		// 드래그한 좌표를 현재 패널 경계 설정에 반영한다.
		void MoveBoundary(int x, int y);
		// 패널 경계를 기본 비율로 되돌린다.
		void ResetPanelLayout();

	public:
		PanelWindow() = default;
		~PanelWindow();

		bool IsCloseRequested() const { return closeRequested; }
		bool IsInputFocused() const { return window && GetForegroundWindow() == window; }

		// 패널 창에서 사용할 메뉴바를 연결한다.
		void AttachMenuBar(MenuBar& menu);
		// 창 이동이나 패널 경계 드래그 종료 콜백을 연결한다.
		void SetInteractionFinishedCallback(std::function<void()> callback) { interactionFinishedCallback = std::move(callback); }
		// 메뉴바 모달 루프 중 렌더링을 유지할 콜백을 연결한다.
		void SetMenuFrameCallback(std::function<bool()> callback) { menuFrameCallback = std::move(callback); }
		// 패널과 제목, 배치 영역을 패널 창에 등록한다.
		void RegisterPanel(Panel& panel, std::string titleKey, PanelWindowArea area, bool visible = true);
		// 패널 창과 등록된 패널 컨트롤을 생성해 표시한다.
		bool Show();
		// 패널 프레임과 내부 컨트롤의 표시 상태를 갱신한다.
		void UpdatePanelVisibility(const Panel& panel, bool visible);
		// 패널 창에 쌓인 Win32 메시지를 처리한다.
		void Poll() const;
		// 현재 언어에 맞춰 창, 메뉴, 패널 제목과 컨트롤 문구를 갱신한다.
		void RefreshLanguage() const;
		// 패널 창과 등록된 패널 컨트롤을 해제한다.
		void Destroy();
	};
}
