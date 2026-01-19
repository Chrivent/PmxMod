#pragma once

#include <string>
#include <filesystem>
#include <glm/gtc/quaternion.hpp>

struct ma_engine;
struct ma_sound;

glm::mat4 InvZ(const glm::mat4& m);

struct UnicodeUtil {
    static std::string WStringToUtf8(const std::wstring& w);
    static std::string SjisToUtf8(const char* sjis);
};

struct PathUtil {
    static std::filesystem::path GetExecutablePath();
    static std::string GetExt(const std::filesystem::path& path);
};

struct MusicUtil {
	MusicUtil();
	~MusicUtil();

	bool Init(const std::filesystem::path& path);
	bool HasMusic() const;
	std::pair<float, float> PullTimes();

private:
	void Uninit();

	std::unique_ptr<ma_engine> m_engine;
	std::unique_ptr<ma_sound>  m_sound;
	bool   m_hasMusic = false;
	double m_prevTimeSec = 0.0;
	float m_volume = 0.1f;
};
