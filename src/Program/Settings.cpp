#include "Program/Settings.h"

#include <fstream>
#include <vector>

#include <nlohmann/json.hpp>
#include <windows.h>

namespace Chrivent {
	std::filesystem::path Settings::ResolveFilePath() {
		DWORD bufferLength = MAX_PATH;
		std::vector<wchar_t> path(bufferLength);
		while (true) {
			const DWORD length = GetModuleFileNameW(nullptr, path.data(), bufferLength);
			if (length < bufferLength - 1)
				return std::filesystem::path(std::wstring(path.data(), length)).parent_path() / "settings.json";
			bufferLength *= 2;
			path.resize(bufferLength);
		}
	}

	PanelLayoutSettings Settings::LoadPanelLayout() {
		std::ifstream stream(ResolveFilePath(), std::ios::binary);
		if (!stream)
			return {};
		const auto json = nlohmann::json::parse(stream, nullptr, false);
		if (!json.is_object() || !json.contains("panel_layout"))
			return {};
		const auto& panelLayout = json["panel_layout"];
		if (!panelLayout.is_object())
			return {};
		PanelLayoutSettings layout;
		layout.leftWidth = panelLayout.value("left_width", 0);
		layout.rightWidth = panelLayout.value("right_width", 0);
		layout.bottomHeight = panelLayout.value("bottom_height", 0);
		return layout;
	}

	void Settings::SavePanelLayout(const PanelLayoutSettings& layout) {
		nlohmann::json json;
		json["panel_layout"] = {
			{"left_width", layout.leftWidth},
			{"right_width", layout.rightWidth},
			{"bottom_height", layout.bottomHeight}
		};
		std::ofstream stream(ResolveFilePath(), std::ios::binary);
		if (stream)
			stream << json.dump(2);
	}

	void Settings::ResetPanelLayout() {
		std::error_code error;
		std::filesystem::remove(ResolveFilePath(), error);
	}
}
