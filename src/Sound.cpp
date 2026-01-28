#include "Sound.h"

#define MINIAUDIO_IMPLEMENTATION
#include "../external/miniaudio.h"

Sound::Sound() {
    m_engine = std::make_unique<ma_engine>();
    m_sound = std::make_unique<ma_sound>();
}

Sound::~Sound() {
    Uninit();
}

bool Sound::Init(const std::filesystem::path& path) {
    Uninit();
    if (path.empty())
        return false;
    if (ma_engine_init(nullptr, m_engine.get()) != MA_SUCCESS)
        return false;
    if (ma_sound_init_from_file_w(m_engine.get(), path.wstring().c_str(),
        0, nullptr, nullptr, m_sound.get()) != MA_SUCCESS) {
        ma_engine_uninit(m_engine.get());
        return false;
    }
    ma_sound_set_volume(m_sound.get(), m_volume);
    ma_sound_start(m_sound.get());
    m_hasSound = true;
    m_prevTimeSec = 0.0;
    return true;
}

std::pair<float, float> Sound::PullTimes() {
    if (!m_hasSound)
        return { 0.f, 0.f };
    ma_uint64 frames{};
    if (ma_sound_get_cursor_in_pcm_frames(m_sound.get(), &frames) != MA_SUCCESS)
        return { 0.f, static_cast<float>(m_prevTimeSec) };
    const double sr = ma_engine_get_sample_rate(m_engine.get());
    const double t = sr > 0.0 ? static_cast<double>(frames) / sr : m_prevTimeSec;
    double dt = t - m_prevTimeSec;
    if (dt < 0.0)
        dt = 0.0;
    m_prevTimeSec = t;
    return { static_cast<float>(dt), static_cast<float>(t) };
}

void Sound::Stop() {
    Uninit();
}

void Sound::Uninit() {
    if (!m_hasSound)
        return;
    ma_sound_uninit(m_sound.get());
    ma_engine_uninit(m_engine.get());
    m_hasSound = false;
    m_prevTimeSec = 0.0;
    m_engine.reset();
    m_sound.reset();
    m_engine = std::make_unique<ma_engine>();
    m_sound = std::make_unique<ma_sound>();
}
