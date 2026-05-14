#include "Config.h"

#include <fstream>
#include <string>

namespace Chrivent {
	bool SceneConfig::Load(const std::filesystem::path& filepath) {
		std::ifstream in(filepath, std::ios::binary);
		if (!in)
			return false;
		std::string magic;
		if (!(in >> magic) || magic != "PmxModScene")
			return false;
		std::string line;
		std::getline(in, line);
		SceneConfig loaded;
		auto ReadLine = [&in, &line] {
			if (!std::getline(in, line))
				return false;
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			return true;
		};
		auto PathFromUtf8 = [](const std::string& utf8) {
			std::u8string u8;
			u8.reserve(utf8.size());
			for (const unsigned char c : utf8)
				u8.push_back(static_cast<char8_t>(c));
			return std::filesystem::path(u8);
		};
		if (!ReadLine())
			return false;
		size_t tab = line.find('\t');
		if (tab == std::string::npos || line.substr(0, tab) != "camera")
			return false;
		loaded.cameraAnim = PathFromUtf8(line.substr(tab + 1));
		if (!ReadLine())
			return false;
		tab = line.find('\t');
		if (tab == std::string::npos || line.substr(0, tab) != "music")
			return false;
		loaded.musicPath = PathFromUtf8(line.substr(tab + 1));
		if (!ReadLine())
			return false;
		tab = line.find('\t');
		if (tab == std::string::npos || line.substr(0, tab) != "models")
			return false;
		const size_t modelCount = std::stoull(line.substr(tab + 1));
		loaded.modelConfigs.reserve(modelCount);
		for (size_t i = 0; i < modelCount; i++) {
			ModelConfig model;
			if (!ReadLine())
				return false;
			const size_t tab1 = line.find('\t');
			const size_t tab2 = line.find('\t', tab1 + 1);
			const size_t tab3 = line.find('\t', tab2 + 1);
			if (tab1 == std::string::npos || tab2 == std::string::npos || tab3 == std::string::npos ||
				line.substr(0, tab1) != "model")
				return false;
			model.scale = std::stof(line.substr(tab1 + 1, tab2 - tab1 - 1));
			const size_t animCount = std::stoull(line.substr(tab2 + 1, tab3 - tab2 - 1));
			model.modelPath = PathFromUtf8(line.substr(tab3 + 1));
			model.animPaths.reserve(animCount);
			for (size_t j = 0; j < animCount; j++) {
				if (!ReadLine())
					return false;
				tab = line.find('\t');
				if (tab == std::string::npos || line.substr(0, tab) != "anim")
					return false;
				model.animPaths.emplace_back(PathFromUtf8(line.substr(tab + 1)));
			}
			loaded.modelConfigs.emplace_back(std::move(model));
		}
		*this = loaded;
		return true;
	}

	bool SceneConfig::Save(const std::filesystem::path& filepath) const {
		std::ofstream out(filepath, std::ios::binary);
		if (!out)
			return false;
		out << "PmxModScene\n";
		const auto camera = cameraAnim.u8string();
		out << "camera\t";
		out.write(reinterpret_cast<const char*>(camera.data()), static_cast<std::streamsize>(camera.size()));
		out << '\n';
		const auto music = musicPath.u8string();
		out << "music\t";
		out.write(reinterpret_cast<const char*>(music.data()), static_cast<std::streamsize>(music.size()));
		out << '\n';
		out << "models\t" << modelConfigs.size() << '\n';
		for (const auto& [modelPath, animPaths, scale] : modelConfigs) {
			const auto model = modelPath.u8string();
			out << "model\t" << scale << '\t' << animPaths.size() << '\t';
			out.write(reinterpret_cast<const char*>(model.data()), static_cast<std::streamsize>(model.size()));
			out << '\n';
			for (const auto& animPath : animPaths) {
				const auto anim = animPath.u8string();
				out << "anim\t";
				out.write(reinterpret_cast<const char*>(anim.data()), static_cast<std::streamsize>(anim.size()));
				out << '\n';
			}
		}
		return static_cast<bool>(out);
	}
}
