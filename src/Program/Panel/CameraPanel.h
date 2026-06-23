#pragma once

#include "Program/Panel/Panel.h"

#include <string>
#include <vector>

namespace Chrivent {
	class CameraPanel final : public Panel {
		UINT_PTR shaderListId = 0;
		int selectedShaderIndex = -1;
		int pendingSelectedShaderIndex = -1;
		HWND shaderList = nullptr;
		std::vector<std::wstring> shaderNames;

		// 현재 셰이더 이름 목록을 리스트 컨트롤에 반영한다.
		void RefreshShaderList() const;

	public:
		CameraPanel() = default;

		void SetShaderListId(const UINT_PTR listId) { shaderListId = listId; }

		// 부모 윈도우 아래에 카메라 패널 컨트롤을 생성한다.
		void Create(HWND parent) override;
		// 패널 크기에 맞춰 셰이더 목록 배치를 갱신한다.
		void Resize(const RECT& clientRect) override;
		// 카메라 패널의 셰이더 목록 표시 상태를 갱신한다.
		void UpdateVisibility(bool visible) const;
		// 셰이더 목록의 선택 변경 명령을 처리한다.
		bool HandleCommand(UINT_PTR commandId, int notificationCode) override;
		// 카메라 패널 컨트롤 핸들을 정리한다.
		void Destroy() override;
		// 선택된 셰이더 인덱스를 반환하고 대기 중인 요청을 초기화한다.
		bool ConsumeSelectedShaderIndex(size_t& shaderIndex);
		// 검색된 셰이더 이름과 현재 선택을 패널에 반영한다.
		void UpdateShaderNames(const std::vector<std::wstring>& names, size_t selectedIndex);
	};
}
