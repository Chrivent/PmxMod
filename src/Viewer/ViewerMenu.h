#pragma once

#include "../Program/Config.h"

#include <windows.h>

namespace Chrivent {
	class ViewerMenu {
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
		explicit ViewerMenu(SceneConfig& config);

		// ?뚯씪 硫붾돱瑜??뚮뜑留?李??곷떒 硫붾돱??異붽??쒕떎.
		static void AddMenu(HMENU menu);
		// ?뚯씪 ??붿긽?먯쓽 遺紐④? ???뚮뜑留?李??몃뱾???곌껐?쒕떎.
		void AttachOwner(HWND owner);
		// 硫붾돱 紐낅졊??泥섎━?쒕떎.
		bool HandleCommand(int commandId);
		// ?몃??먯꽌 ?꾨떖?????ㅼ젙??硫붾돱 ?곹깭??諛섏쁺?쒕떎.
		void ApplySceneConfig(const SceneConfig& cfg);
		// ??蹂寃??뚮옒洹몃? 珥덇린?뷀븳??
		void Reset();
		// ???ㅼ젙 蹂寃??뚮옒洹몃? 諛섑솚?섍퀬 珥덇린?뷀븳??
		bool ConsumeSceneConfigDirty();
	};
}
