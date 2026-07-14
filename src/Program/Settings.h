#pragma once

#include <filesystem>

namespace Chrivent {
	// 패널 경계와 창 배치에 필요한 사용자 설정을 보관한다.
	struct PanelLayoutSettings {
		int leftWidth = 0;
		int rightWidth = 0;
		int bottomHeight = 0;
	};

	// 프로그램 설정 파일을 읽고 저장한다.
	class Settings {
		static std::filesystem::path ResolveFilePath();

	public:
		// 별도 JSON 파일에서 프로그램 설정을 읽는다.
		static PanelLayoutSettings LoadPanelLayout();
		// 현재 패널 경계 설정을 별도 JSON 파일에 저장한다.
		static void SavePanelLayout(const PanelLayoutSettings& layout);
		// 저장된 패널 경계 설정을 삭제해 기본 배치로 되돌린다.
		static void ResetPanelLayout();
	};
}
