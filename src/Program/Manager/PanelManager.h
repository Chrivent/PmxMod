#pragma once

#include <windows.h>

#include "../Config.h"
#include "../Panel/ScenePanel.h"
#include "../Panel/SoundPanel.h"

namespace Chrivent {
	class Sound;

	class PanelManager {
		SceneConfig sceneConfigStorage;
		ScenePanel scenePanel;
		SoundPanel soundPanel;
		HWND controlWindow = nullptr;

		// ???⑤꼸 李??ш린??留욎떠 ?대? ?덈룄??而⑦듃濡?諛곗튂瑜?媛깆떊?쒕떎.
		void ResizeControlWindow();
		// ???⑤꼸 李쎌쓽 Win32 硫붿떆吏瑜?泥섎━?쒕떎.
		static LRESULT CALLBACK ControlWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	public:
		SceneConfig& sceneConfig;

		PanelManager();
		~PanelManager();

		// ?몃??먯꽌 ?꾨떖?????ㅼ젙???⑤꼸 ?곹깭??諛섏쁺?쒕떎.
		void ApplySceneConfig(const SceneConfig& cfg);
		// ?⑤꼸 ?곹깭瑜?湲곕낯媛믪쑝濡?珥덇린?뷀븳??
		void Reset();
		// ???⑤꼸怨??ъ슫???⑤꼸 李쎌쓣 ?앹꽦?섍굅???ㅼ떆 ?쒖떆?쒕떎.
		bool OpenPanelWindows();
		// ?⑤꼸 李쎈뱾???볦씤 Win32 硫붿떆吏瑜?泥섎━?쒕떎.
		void PollPanelWindows() const;
		// ?⑤꼸 李쎈뱾???뚭눼?섍퀬 愿???몃뱾??珥덇린?뷀븳??
		void DestroyPanelWindows();
		// ???ㅼ젙 蹂寃??뚮옒洹몃? 諛섑솚?섍퀬 珥덇린?뷀븳??
		bool ConsumeSceneConfigDirty();
		// ?ъ슫???⑤꼸??議곗젅???ъ슫??媛앹껜瑜??곌껐?쒕떎.
		void BindSound(Sound& sound);
	};
}
