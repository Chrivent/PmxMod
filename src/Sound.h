#pragma once

#include <filesystem>
#include <memory>
#include <utility>

struct ma_engine;
struct ma_sound;

struct Sound {
    Sound();
    ~Sound();

    bool m_hasSound = false;
    float m_volume = 0.1f;

    bool Init(const std::filesystem::path& path, bool loop);
    double GetLengthSec() const { return m_lengthSec; }
    std::pair<float, float> PullTimes();
    void Stop();

private:
    std::unique_ptr<ma_engine> m_engine;
    std::unique_ptr<ma_sound>  m_sound;
    double m_prevTimeSec = 0.0;
    double m_lengthSec = 0.0;

    void Uninit();
};
