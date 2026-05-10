#include "Sound.h"

#include <algorithm>

#define MINIAUDIO_IMPLEMENTATION
#include "../external/miniaudio.h"

Sound::Sound() {
    engine = std::make_unique<ma_engine>();
    sound = std::make_unique<ma_sound>();
}

Sound::~Sound() {
    Uninit();
}

bool Sound::Init(const std::filesystem::path& path, const bool loop) {
    Uninit();
    if (path.empty())
        return false;
    if (ma_engine_init(nullptr, engine.get()) != MA_SUCCESS)
        return false;
    if (ma_sound_init_from_file_w(engine.get(), path.wstring().c_str(),
        0, nullptr, nullptr, sound.get()) != MA_SUCCESS) {
        ma_engine_uninit(engine.get());
        return false;
    }
    ma_uint64 lengthFrames = 0;
    if (ma_sound_get_length_in_pcm_frames(sound.get(), &lengthFrames) == MA_SUCCESS) {
        const double sr = ma_engine_get_sample_rate(engine.get());
        lengthSec = sr > 0.0 ? static_cast<double>(lengthFrames) / sr : 0.0;
    } else {
        lengthSec = 0.0;
    }
    ma_sound_set_looping(sound.get(), loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(sound.get(), volume);
    ma_sound_start(sound.get());
    hasSound = true;
    prevTimeSec = 0.0;
    return true;
}

void Sound::ApplyVolume() {
    volume = std::clamp(volume, 0.0f, 1.0f);
    if (hasSound) {
        ma_sound_set_volume(sound.get(), volume);
    }
}

std::pair<float, float> Sound::PullTimes() {
    if (!hasSound)
        return { 0.f, 0.f };
    ma_uint64 frames{};
    if (ma_sound_get_cursor_in_pcm_frames(sound.get(), &frames) != MA_SUCCESS)
        return { 0.f, static_cast<float>(prevTimeSec) };
    const double sr = ma_engine_get_sample_rate(engine.get());
    const double t = sr > 0.0 ? static_cast<double>(frames) / sr : prevTimeSec;
    double dt = t - prevTimeSec;
    if (dt < 0.0)
        dt = 0.0;
    prevTimeSec = t;
    return { static_cast<float>(dt), static_cast<float>(t) };
}

void Sound::Pause() const {
    if (!hasSound)
        return;
    ma_sound_stop(sound.get());
}

void Sound::Resume() {
    if (!hasSound)
        return;
    ma_uint64 frames{};
    if (ma_sound_get_cursor_in_pcm_frames(sound.get(), &frames) == MA_SUCCESS) {
        const double sr = ma_engine_get_sample_rate(engine.get());
        prevTimeSec = sr > 0.0 ? static_cast<double>(frames) / sr : prevTimeSec;
    }
    ma_sound_start(sound.get());
}

void Sound::Stop() {
    Uninit();
}

void Sound::Uninit() {
    if (!hasSound)
        return;
    ma_sound_uninit(sound.get());
    ma_engine_uninit(engine.get());
    hasSound = false;
    prevTimeSec = 0.0;
    lengthSec = 0.0;
    engine.reset();
    sound.reset();
    engine = std::make_unique<ma_engine>();
    sound = std::make_unique<ma_sound>();
}

