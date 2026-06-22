#include "Sound.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>

namespace Chrivent {
    void Sound::UnInit() {
        waveform.minimums.clear();
        waveform.maximums.clear();
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

    void Sound::BuildWaveform(const std::filesystem::path& path) {
        constexpr ma_uint32 timelineFrameRate = 30;
        constexpr ma_uint32 waveformSamplesPerFrame = 48;
        constexpr ma_uint32 sourceSamplesPerPeak = 10;
        constexpr ma_uint32 waveformSampleRate = timelineFrameRate * waveformSamplesPerFrame * sourceSamplesPerPeak;
        constexpr ma_uint64 samplesPerPeak = sourceSamplesPerPeak;
        constexpr ma_uint64 bufferFrameCount = 4096;
        ma_decoder decoder{};
        const ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 1, waveformSampleRate);
        if (ma_decoder_init_file_w(path.c_str(), &decoderConfig, &decoder) != MA_SUCCESS)
            return;
        float samples[bufferFrameCount]{};
        waveform.samplesPerFrame = waveformSamplesPerFrame;
        ma_uint64 peakSampleCount = 0;
        float minimum = 1.0f;
        float maximum = -1.0f;
        while (true) {
            ma_uint64 framesRead = 0;
            if (ma_decoder_read_pcm_frames(&decoder, samples, bufferFrameCount, &framesRead) != MA_SUCCESS ||
                framesRead == 0)
                break;
            for (ma_uint64 index = 0; index < framesRead; index++) {
                minimum = (std::min)(minimum, samples[index]);
                maximum = (std::max)(maximum, samples[index]);
                peakSampleCount++;
                if (peakSampleCount < samplesPerPeak)
                    continue;
                waveform.minimums.emplace_back(std::clamp(minimum, -1.0f, 1.0f));
                waveform.maximums.emplace_back(std::clamp(maximum, -1.0f, 1.0f));
                peakSampleCount = 0;
                minimum = 1.0f;
                maximum = -1.0f;
            }
        }
        if (peakSampleCount > 0) {
            waveform.minimums.emplace_back(std::clamp(minimum, -1.0f, 1.0f));
            waveform.maximums.emplace_back(std::clamp(maximum, -1.0f, 1.0f));
        }
        ma_decoder_uninit(&decoder);
    }

    Sound::Sound() {
        engine = std::make_unique<ma_engine>();
        sound = std::make_unique<ma_sound>();
    }

    Sound::~Sound() {
        UnInit();
    }

    void Sound::SetVolume(const float value) {
        volume = std::clamp(value, 0.0f, 1.0f);
        if (hasSound)
            ma_sound_set_volume(sound.get(), volume);
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
            lengthSec = sr > 0.0 ? lengthFrames / sr : 0.0;
        } else
            lengthSec = 0.0;
        ma_sound_set_looping(sound.get(), loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(sound.get(), volume);
        hasSound = true;
        playing = false;
        prevTimeSec = 0.0;
        BuildWaveform(path);
        return true;
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
            time = prevTimeSec;
            return;
        }
        const double sr = ma_engine_get_sample_rate(engine.get());
        const double t = sr > 0.0 ? frames / sr : prevTimeSec;
        double dt = t - prevTimeSec;
        if (dt < 0.0)
            dt = 0.0;
        prevTimeSec = t;
        deltaTime = dt;
        time = t;
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
            prevTimeSec = sr > 0.0 ? frames / sr : prevTimeSec;
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
        const double seekSeconds = seconds;
        const double clampedTime = lengthSec > 0.0
            ? std::clamp(seekSeconds, 0.0, lengthSec)
            : (std::max)(0.0f, seconds);
        const auto frame = clampedTime * sr;
        if (ma_sound_seek_to_pcm_frame(sound.get(), frame) == MA_SUCCESS) {
            prevTimeSec = clampedTime;
            if (playing)
                ma_sound_start(sound.get());
        }
    }

}
