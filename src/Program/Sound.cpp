#include "Sound.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace Chrivent {
    void Sound::UnInit() {
        if (!hasSound)
            return;
        ma_sound_uninit(sound.get());
        ma_engine_uninit(engine.get());
        hasSound = false;
        playing = false;
        prevTimeSec = 0.0;
        lengthSec = 0.0;
        engine.reset();
        sound.reset();
        engine = std::make_unique<ma_engine>();
        sound = std::make_unique<ma_sound>();
    }

    Sound::Sound() {
        engine = std::make_unique<ma_engine>();
        sound = std::make_unique<ma_sound>();
    }

    Sound::~Sound() {
        UnInit();
    }

    bool Sound::Init(const std::filesystem::path& path, const bool loop) {
        UnInit();
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
        } else
            lengthSec = 0.0;
        ma_sound_set_looping(sound.get(), loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(sound.get(), volume);
        hasSound = true;
        playing = false;
        prevTimeSec = 0.0;
        return true;
    }

    void Sound::SetVolume(const float value) {
        volume = std::clamp(value, 0.0f, 1.0f);
        if (hasSound)
            ma_sound_set_volume(sound.get(), volume);
    }

    void Sound::PullTimes(float& deltaTime, float& time) {
        if (!hasSound) {
            deltaTime = 0.0f;
            time = 0.0f;
            return;
        }
        ma_uint64 frames{};
        if (ma_sound_get_cursor_in_pcm_frames(sound.get(), &frames) != MA_SUCCESS) {
            deltaTime = 0.0f;
            time = static_cast<float>(prevTimeSec);
            return;
        }
        const double sr = ma_engine_get_sample_rate(engine.get());
        const double t = sr > 0.0 ? static_cast<double>(frames) / sr : prevTimeSec;
        double dt = t - prevTimeSec;
        if (dt < 0.0)
            dt = 0.0;
        prevTimeSec = t;
        deltaTime = static_cast<float>(dt);
        time = static_cast<float>(t);
    }

    void Sound::Pause() {
        if (!hasSound)
            return;
        ma_sound_stop(sound.get());
        playing = false;
    }

    void Sound::Resume() {
        if (!hasSound)
            return;
        ma_uint64 frames{};
        if (ma_sound_get_cursor_in_pcm_frames(sound.get(), &frames) == MA_SUCCESS) {
            const double sr = ma_engine_get_sample_rate(engine.get());
            prevTimeSec = sr > 0.0 ? static_cast<double>(frames) / sr : prevTimeSec;
        }
        if (ma_sound_start(sound.get()) == MA_SUCCESS)
            playing = true;
    }

    void Sound::SeekSeconds(const float seconds) {
        if (!hasSound)
            return;
        const double sr = ma_engine_get_sample_rate(engine.get());
        if (sr <= 0.0)
            return;
        const double clampedTime = lengthSec > 0.0
            ? std::clamp(static_cast<double>(seconds), 0.0, lengthSec)
            : (std::max)(0.0, static_cast<double>(seconds));
        const auto frame = static_cast<ma_uint64>(clampedTime * sr);
        if (ma_sound_seek_to_pcm_frame(sound.get(), frame) == MA_SUCCESS) {
            prevTimeSec = clampedTime;
            if (playing)
                ma_sound_start(sound.get());
        }
    }

    void Sound::Stop() {
        UnInit();
    }
}
