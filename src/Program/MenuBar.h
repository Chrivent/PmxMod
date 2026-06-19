#pragma once

#include "Config.h"
#include "RendererType.h"

#include <windows.h>

namespace Chrivent {
	class MenuBar {
		static constexpr int kOpenButtonId = 1000;
		static constexpr int kSaveButtonId = 1001;
		static constexpr int kOpenGlRendererId = 1100;
		static constexpr int kDirectX11RendererId = 1101;
		static constexpr int kDirectX12RendererId = 1102;
		static constexpr int kVulkanRendererId = 1103;

		SceneConfig& sceneConfig;
		std::filesystem::path sceneFilePath;
		bool sceneConfigDirty = false;
		RendererType rendererType = RendererType::OpenGL;
		bool rendererDirty = false;
		HWND ownerWindow = nullptr;

		// 현재 씬 설정을 지정한 파일에 저장한다.
		bool SaveSceneConfig(const std::filesystem::path& filepath) const;
		// 지정한 파일에서 씬 설정을 읽고 변경 상태를 기록한다.
		bool LoadSceneConfig(const std::filesystem::path& filepath);
		// 씬 설정 파일을 선택하는 열기 대화상자를 표시한다.
		void ShowOpenSceneDialog();
		// 씬 설정을 저장할 경로를 선택하는 대화상자를 표시한다.
		void ShowSaveSceneDialog();
		// 사용할 렌더러를 선택하고 변경 상태를 기록한다.
		void SelectRenderer(RendererType renderer);
		// 현재 렌더러에 맞춰 메뉴의 선택 표시를 갱신한다.
		void UpdateRendererMenuCheck() const;

	public:
		explicit MenuBar(SceneConfig& config);

		RendererType GetRendererType() const { return rendererType; }
		void SetOwnerWindow(const HWND owner) { ownerWindow = owner; }

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
	};
}
