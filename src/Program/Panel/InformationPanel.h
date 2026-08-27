#pragma once

#include "Program/Panel/Panel.h"

#include <string>
#include <vector>

namespace Chrivent {
	// 정보 패널에 표시할 번역 레이블과 값을 보관한다.
	struct InformationField {
		std::string labelKey;
		std::wstring value;
	};

	// 현재 선택된 모델이나 이펙트의 정보를 상시 유지되는 읽기 전용 영역에 표시한다.
	class InformationPanel final : public Panel {
		HWND informationText = nullptr;
		std::vector<InformationField> fields;

		// 현재 언어의 레이블과 저장된 값을 정보 텍스트에 반영한다.
		void RefreshText() const;

	public:
		InformationPanel() = default;

		// 부모 윈도우 아래에 읽기 전용 정보 컨트롤을 생성한다.
		void Create(HWND parent) override;
		// 패널 크기에 맞춰 정보 컨트롤을 배치한다.
		void Resize(const RECT& clientRect) override;
		// 정보 컨트롤의 표시 상태를 갱신한다.
		void UpdateVisibility(bool visible) const override;
		// 현재 언어로 정보 레이블을 다시 구성한다.
		void UpdateLanguage() override;
		// 정보 컨트롤과 저장된 필드를 정리한다.
		void Destroy() override;
		// 선택 항목의 정보 필드를 패널에 반영한다.
		void ApplyFields(std::vector<InformationField> informationFields);
		// 선택 정보와 표시 텍스트를 비운다.
		void Clear();
	};
}
