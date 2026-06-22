#pragma once

#include "Config.h"
#include "Language.h"
#include "RendererType.h"

#include <windows.h>

namespace Chrivent {
	class MenuBar {
		static constexpr int kNewButtonId = 999;
		static constexpr int kOpenButtonId = 1000;
		static constexpr int kSaveButtonId = 1001;
		static constexpr int kOpenGlRendererId = 1100;
		static constexpr int kDirectX11RendererId = 1101;
		static constexpr int kDirectX12RendererId = 1102;
		static constexpr int kVulkanRendererId = 1103;
		static constexpr int kPhysicsEnabledId = 1150;
		static constexpr int kFpsViewId = 1200;
		static constexpr int kResetPanelLayoutId = 1201;
		static constexpr int kEnglishLanguageId = 1300;
		static constexpr int kKoreanLanguageId = 1301;
		static constexpr int kJapaneseLanguageId = 1302;
		static constexpr int kChineseLanguageId = 1303;

		SceneConfig& sceneConfig;
		std::filesystem::path sceneFilePath;
		bool sceneConfigDirty = false;
		RendererType rendererType = RendererType::OpenGL;
		bool rendererDirty = false;
		bool physicsEnabled = true;
		bool fpsVisible = false;
		bool languageDirty = false;
		bool panelLayoutResetRequested = false;
		HWND ownerWindow = nullptr;

		// 현재 씬 설정을 지정한 파일에 저장한다.
		bool SaveSceneConfig(const std::filesystem::path& filepath) const;
		// 지정한 파일에서 씬 설정을 읽고 변경 상태를 기록한다.
		bool LoadSceneConfig(const std::filesystem::path& filepath);
		// 모델, 모션, 카메라와 음악이 없는 새 씬을 적용한다.
		void CreateNewScene();
		// 씬 설정 파일을 선택하는 열기 대화상자를 표시한다.
		void ShowOpenSceneDialog();
		// 씬 설정을 저장할 경로를 선택하는 대화상자를 표시한다.
		void ShowSaveSceneDialog();
		// 사용할 렌더러를 선택하고 변경 상태를 기록한다.
		void SelectRenderer(RendererType renderer);
		// 현재 렌더러에 맞춰 메뉴의 선택 표시를 갱신한다.
		void UpdateRendererMenuCheck() const;
		// 현재 언어에 맞춰 메뉴의 선택 표시를 갱신한다.
		void UpdateLanguageMenuCheck() const;
		// GUI 언어를 선택하고 변경 상태를 기록한다.
		void SelectLanguage(LanguageType type);

	public:
		explicit MenuBar(SceneConfig& config);

		RendererType GetRendererType() const { return rendererType; }
		bool IsPhysicsEnabled() const { return physicsEnabled; }
		bool IsFpsVisible() const { return fpsVisible; }
		void SetOwnerWindow(const HWND owner) { ownerWindow = owner; }

		// 현재 렌더러를 변경 요청 없이 메뉴 상태에 반영한다.
		void ApplyRenderer(RendererType renderer);
		// 설정 창 상단 메뉴를 구성한다.
		void AddMenu(HMENU menu) const;
		// 메뉴 명령을 처리한다.
		bool HandleCommand(int commandId);
		// 외부에서 전달된 씬 설정을 메뉴 상태에 반영한다.
		void ApplySceneConfig(const SceneConfig& cfg);
		// 씬 변경 플래그를 초기화한다.
		void Reset();
		// 씬 설정 변경 플래그를 반환하고 초기화한다.
		bool ConsumeSceneConfigDirty();
		// 렌더러 변경 플래그를 반환하고 초기화한다.
		bool ConsumeRendererDirty();
		// 언어 변경 플래그를 반환하고 초기화한다.
		bool ConsumeLanguageDirty();
		// 패널 경계 초기화 요청을 반환하고 내부 상태를 초기화한다.
		bool ConsumePanelLayoutResetRequest();
	};
}
