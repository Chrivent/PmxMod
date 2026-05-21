#pragma once

#include "Config.h"

#include <windows.h>

namespace Chrivent {
	class MenuBar {
		static constexpr int kOpenButtonId = 1000;
		static constexpr int kSaveButtonId = 1001;

		SceneConfig& sceneConfig;
		std::filesystem::path sceneFilePath;
		bool sceneConfigDirty = false;
		HWND ownerWindow = nullptr;

		bool SaveSceneConfig(const std::filesystem::path& filepath) const;
		bool LoadSceneConfig(const std::filesystem::path& filepath);
		void ShowOpenSceneDialog();
		void ShowSaveSceneDialog();

	public:
		explicit MenuBar(SceneConfig& config);

		// 파일 메뉴를 렌더링 창 상단 메뉴에 추가한다.
		static void AddMenu(HMENU menu);
		// 파일 대화상자의 부모가 될 렌더링 창 핸들을 연결한다.
		void AttachOwner(HWND owner);
		// 메뉴 명령을 처리한다.
		bool HandleCommand(int commandId);
		// 외부에서 전달된 씬 설정을 메뉴 상태에 반영한다.
		void ApplySceneConfig(const SceneConfig& cfg);
		// 씬 변경 플래그를 초기화한다.
		void Reset();
		// 씬 설정 변경 플래그를 반환하고 초기화한다.
		bool ConsumeSceneConfigDirty();
	};
}
