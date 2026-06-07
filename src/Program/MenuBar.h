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

		bool SaveSceneConfig(const std::filesystem::path& filepath) const;
		bool LoadSceneConfig(const std::filesystem::path& filepath);
		void ShowOpenSceneDialog();
		void ShowSaveSceneDialog();
		void SelectRenderer(RendererType renderer);
		void UpdateRendererMenuCheck() const;

	public:
		explicit MenuBar(SceneConfig& config);

		// 설정 창 상단 메뉴를 구성한다.
		void AddMenu(HMENU menu) const;
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
		RendererType GetRendererType() const { return rendererType; }
		bool ConsumeRendererDirty();
	};
}
