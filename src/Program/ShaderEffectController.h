#pragma once

#include "Program/Shader/ShaderPackage.h"
#include "Viewer/Error/GraphicsError.h"

#include <filesystem>
#include <vector>

namespace Chrivent {
	class PanelManager;
	class Viewer;

	// 셰이더 패키지 검색, 선택 상태, 패널 표시와 뷰어 적용을 조정한다.
	class ShaderEffectController {
		// 패널의 단일 효과 행이 참조하는 패키지와 효과 인덱스를 보관한다.
		struct EffectEntry {
			size_t packageIndex = 0;
			size_t effectIndex = 0;
		};

		// 유효한 패키지와 효과 포인터를 하나의 조회 결과로 묶는다.
		struct EffectReference {
			const ShaderPackage* package = nullptr;
			const EffectDefinition* effect = nullptr;
		};

		std::vector<ShaderPackage> packages;
		std::vector<EffectEntry> effectEntries;
		std::vector<bool> effectEnabled;
		size_t selectedEffectIndex = 0;

		// 검색된 패키지의 모든 효과를 패널 행 순서로 평탄화한다.
		void BuildEffectEntries();
		// 패널 효과 인덱스를 검증하고 해당 패키지와 효과를 반환한다.
		EffectReference ResolveEffect(size_t effectIndex) const;
		// 현재 효과 이름, 체크 상태와 내장 셰이더 상태를 패널에 반영한다.
		void RefreshPanel(PanelManager& panelManager, Viewer& viewer) const;
		// 선택한 효과의 패키지 메타데이터를 정보 패널에 표시한다.
		void ShowEffectInformation(PanelManager& panelManager, size_t effectIndex) const;
		// 선택한 효과 이름을 모션 패널에 표시한다.
		void ShowEffectTimeline(PanelManager& panelManager, size_t effectIndex) const;

	public:
		// 패키지를 다시 검색하고 기존 선택·활성 상태를 복구한 뒤 뷰어에 적용한다.
		GraphicsError::Result<void> Reload(const std::filesystem::path& packagesDirectory,
			Viewer& viewer, PanelManager& panelManager);
		// 현재 활성화된 효과 실행 계약을 뷰어에 적용한다.
		GraphicsError::Result<void> Apply(Viewer& viewer) const;
		// 패널의 효과 선택과 내장 셰이더 토글 요청을 소비해 뷰어와 패널에 반영한다.
		GraphicsError::Result<void> ProcessPanelRequests(PanelManager& panelManager, Viewer& viewer);
	};
}
