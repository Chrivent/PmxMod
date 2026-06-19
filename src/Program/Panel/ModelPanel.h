#pragma once

#include "Panel.h"

#include <filesystem>
#include <vector>

namespace Chrivent {
	class ModelPanel final : public Panel {
		int addButtonId = 0;
		HWND parentWindow = nullptr;
		HWND addButton = nullptr;
		HWND modelList = nullptr;
		std::vector<std::filesystem::path> modelPaths;
		std::filesystem::path pendingModelPath;

		// PMX 모델 파일을 선택하는 열기 대화상자를 표시한다.
		void ShowOpenModelDialog();
		// 현재 모델 경로 목록을 리스트 컨트롤에 반영한다.
		void RefreshModelList() const;

	public:
		ModelPanel() = default;

		void SetAddButtonId(const int id) { addButtonId = id; }

		// 부모 윈도우 아래에 모델 패널 컨트롤을 생성한다.
		void Create(HWND parent) override;
		// 패널 크기에 맞춰 모델 패널 컨트롤 배치를 갱신한다.
		void Resize(const RECT& clientRect) override;
		// Add 버튼 명령을 처리해 모델 파일 선택 요청을 만든다.
		bool HandleCommand(int commandId) override;
		// 모델 패널 컨트롤 핸들을 정리한다.
		void Destroy() override;
		// 선택된 모델 경로를 반환하고 대기 중인 요청을 초기화한다.
		bool ConsumeAddModelPath(std::filesystem::path& modelPath);
		// 씬에 배치된 모델 경로 목록을 패널에 반영한다.
		void UpdateModelPaths(const std::vector<std::filesystem::path>& paths);
	};
}
