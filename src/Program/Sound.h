#pragma once

#include <filesystem>
#include <memory>
#include <vector>

struct ma_engine;
struct ma_sound;

namespace Chrivent {
    struct AudioWaveform {
        std::vector<float> minimums;
        std::vector<float> maximums;
        int samplesPerFrame = 10;
    };

    class Sound {
        float volume = 0.5f;
        double lengthSec = 0.0;
        std::unique_ptr<ma_engine> engine;
        std::unique_ptr<ma_sound>  sound;
        double prevTimeSec = 0.0;
        bool playing = false;
        bool hasSound = false;
        AudioWaveform waveform;

        // MiniAudio 엔진과 사운드 객체를 해제한다.
        void UnInit();
        // 오디오 파일을 30fps 타임라인용 단일 채널 피크 데이터로 변환한다.
        void BuildWaveform(const std::filesystem::path& path);
    
    public:
        Sound();
        ~Sound();
    
        Sound(const Sound&) = delete;
        Sound& operator=(const Sound&) = delete;
        Sound(Sound&&) = delete;
        Sound& operator=(Sound&&) = delete;
        
        bool HasSound() const { return hasSound; }
        float GetVolume() const { return volume; }
        double GetLengthSeconds() const { return lengthSec; }
        const AudioWaveform& GetWaveform() const { return waveform; }
        void SetVolume(float value);

        // 오디오 파일을 열고 필요하면 반복 재생으로 준비한다.
        bool Init(const std::filesystem::path& path, bool loop);
        // 이전 호출 시각과 현재 재생 시각을 출력하고 내부 기준 시간을 갱신한다.
        void PullTimes(float& deltaTime, float& time);
        // 재생 중인 사운드를 일시정지한다.
        void Pause();
        // 사운드 재생을 재개한다.
        void Resume();
        // 사운드 재생 위치를 지정한 초로 이동한다.
        void SeekSeconds(float seconds);
        // 사운드를 정지하고 재생 위치를 초기화한다.
        void Stop() { UnInit(); }
    };
}
