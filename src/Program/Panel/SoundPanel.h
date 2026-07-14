#pragma once

#include "Program/Panel/Panel.h"

namespace Chrivent {
	class Sound;

	// 음악 파일 선택과 오디오 파형을 표시한다.
	class SoundPanel final : public Panel {
		UINT_PTR volumeSliderId = 0;
		Sound* sound = nullptr;
		HWND panelWindow = nullptr;
		HWND volumeSlider = nullptr;
		HWND valueText = nullptr;

		// 사운드 패널 윈도우의 Win32 메시지를 처리한다.
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		// 현재 볼륨 값을 텍스트 컨트롤에 표시한다.
		void UpdateValueText() const;
		// 슬라이더 위치를 사운드 볼륨에 반영한다.
		void ApplySliderValue() const;
		// 패널 안에 볼륨 컨트롤들을 만든다.
		void CreateContent(HWND parent);

	public:
		SoundPanel() = default;

		void SetVolumeSliderId(const UINT_PTR id) { volumeSliderId = id; }
		
		// 패널이 조절할 사운드 객체를 연결한다.
		void BindSound(Sound& soundRef);
		// 부모 윈도우 아래에 패널 컨트롤을 생성한다.
		void Create(const HWND parent) override { CreateContent(parent); }
		// 패널 윈도우를 생성하거나 이미 있으면 다시 표시한다.
		void Show();
		// 패널 윈도우에 쌓인 메시지를 처리한다.
		void Poll() const;
		// 패널 크기에 맞춰 볼륨 컨트롤 위치를 갱신한다.
		void Resize(const RECT& clientRect) override;
		// 볼륨 슬라이더 이동을 사운드 볼륨에 반영한다.
		bool HandleScroll(HWND control, int scrollCode) override;
		// 패널 윈도우와 컨트롤 핸들을 정리한다.
		void Destroy() override;
	};
}
