#include "Program/Sound.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace Chrivent {
    void Sound::UnInit() {
        waveform.minimums.clear();
        waveform.maximums.clear();
		if (hasSound) {
			ma_sound_uninit(sound.get());
			ma_engine_uninit(engine.get());
		}
        hasSound = false;
        playing = false;
        prevTimeSec = 0.0;
        lengthSec = 0.0;
        engine.reset();
        sound.reset();
    }

    AudioWaveform Sound::BuildWaveform(const std::filesystem::path& path) {
        constexpr ma_uint32 timelineFrameRate = 30;
        constexpr ma_uint32 waveformSamplesPerFrame = 48;
        constexpr ma_uint32 sourceSamplesPerPeak = 10;
        constexpr ma_uint32 waveformSampleRate = timelineFrameRate * waveformSamplesPerFrame * sourceSamplesPerPeak;
        constexpr ma_uint64 samplesPerPeak = sourceSamplesPerPeak;
        constexpr ma_uint64 bufferFrameCount = 4096;
		AudioWaveform loadedWaveform;
        ma_decoder decoder{};
        const ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 1, waveformSampleRate);
        if (ma_decoder_init_file_w(path.c_str(), &decoderConfig, &decoder) != MA_SUCCESS)
			return loadedWaveform;
        float samples[bufferFrameCount]{};
		loadedWaveform.samplesPerFrame = waveformSamplesPerFrame;
        ma_uint64 peakSampleCount = 0;
        float minimum = 1.0f;
        float maximum = -1.0f;
        while (true) {
            ma_uint64 framesRead = 0;
            if (ma_decoder_read_pcm_frames(&decoder, samples, bufferFrameCount, &framesRead) != MA_SUCCESS ||
                framesRead == 0)
                break;
            for (ma_uint64 index = 0; index < framesRead; index++) {
                minimum = std::min(minimum, samples[index]);
                maximum = std::max(maximum, samples[index]);
                peakSampleCount++;
                if (peakSampleCount < samplesPerPeak)
                    continue;
				loadedWaveform.minimums.emplace_back(std::clamp(minimum, -1.0f, 1.0f));
				loadedWaveform.maximums.emplace_back(std::clamp(maximum, -1.0f, 1.0f));
                peakSampleCount = 0;
                minimum = 1.0f;
                maximum = -1.0f;
            }
        }
        if (peakSampleCount > 0) {
			loadedWaveform.minimums.emplace_back(std::clamp(minimum, -1.0f, 1.0f));
			loadedWaveform.maximums.emplace_back(std::clamp(maximum, -1.0f, 1.0f));
        }
        ma_decoder_uninit(&decoder);
		return loadedWaveform;
    }

	void Sound::MoveFrom(Sound& source) {
		volume = source.volume;
		lengthSec = source.lengthSec;
		engine = std::move(source.engine);
		sound = std::move(source.sound);
		prevTimeSec = source.prevTimeSec;
		playing = source.playing;
		hasSound = source.hasSound;
		waveform = std::move(source.waveform);
		source.lengthSec = 0.0;
		source.prevTimeSec = 0.0;
		source.playing = false;
		source.hasSound = false;
    }

	Sound::Sound() = default;

    Sound::~Sound() {
        UnInit();
    }

	Sound::Sound(Sound&& other) noexcept {
		MoveFrom(other);
	}

	Sound& Sound::operator=(Sound&& other) noexcept {
		if (this != &other) {
			UnInit();
			MoveFrom(other);
		}
		return *this;
	}

    void Sound::ApplyVolume(const float value) {
        volume = std::clamp(value, 0.0f, 1.0f);
        if (hasSound)
            ma_sound_set_volume(sound.get(), volume);
    }

    bool Sound::Init(const std::filesystem::path& path, const bool loop) {
        if (path.empty())
            return false;
		auto loadedEngine = std::make_unique<ma_engine>();
		auto loadedSound = std::make_unique<ma_sound>();
		if (ma_engine_init(nullptr, loadedEngine.get()) != MA_SUCCESS)
            return false;
		if (ma_sound_init_from_file_w(loadedEngine.get(), path.wstring().c_str(),
			0, nullptr, nullptr, loadedSound.get()) != MA_SUCCESS) {
			ma_engine_uninit(loadedEngine.get());
            return false;
		}
        ma_uint64 lengthFrames = 0;
		double loadedLengthSec = 0.0;
		if (ma_sound_get_length_in_pcm_frames(loadedSound.get(), &lengthFrames) == MA_SUCCESS) {
			const double sampleRate = ma_engine_get_sample_rate(loadedEngine.get());
			loadedLengthSec = sampleRate > 0.0 ? lengthFrames / sampleRate : 0.0;
		}
		ma_sound_set_looping(loadedSound.get(), loop ? MA_TRUE : MA_FALSE);
		ma_sound_set_volume(loadedSound.get(), volume);
		AudioWaveform loadedWaveform = BuildWaveform(path);
		UnInit();
		engine = std::move(loadedEngine);
		sound = std::move(loadedSound);
		lengthSec = loadedLengthSec;
		waveform = std::move(loadedWaveform);
        hasSound = true;
        playing = false;
        prevTimeSec = 0.0;
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
			time = static_cast<float>(prevTimeSec);
            return;
        }
        const double sr = ma_engine_get_sample_rate(engine.get());
        const double t = sr > 0.0 ? frames / sr : prevTimeSec;
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
            : std::max(0.0f, seconds);
		const long double targetFrame = static_cast<long double>(clampedTime) * sr;
		if (targetFrame > std::numeric_limits<ma_uint64>::max())
			return;
		const ma_uint64 frame = static_cast<ma_uint64>(targetFrame);
		if (ma_sound_seek_to_pcm_frame(sound.get(), frame) == MA_SUCCESS) {
            prevTimeSec = clampedTime;
            if (playing)
                ma_sound_start(sound.get());
        }
    }
}
