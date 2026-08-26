#pragma once

#include <filesystem>
#include <memory>
#include <vector>

// ReSharper disable once CppInconsistentNaming
struct ma_engine;
// ReSharper disable once CppInconsistentNaming
struct ma_sound;

namespace Chrivent {
    // 오디오 파형 표시용 최소·최대 샘플과 재생 시간을 보관한다.
    struct AudioWaveform {
        std::vector<float> minimums;
        std::vector<float> maximums;
        int samplesPerFrame = 10;
    };

    // 음악 파일의 디코딩, 재생, 탐색과 파형 생성을 관리한다.
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
        static AudioWaveform BuildWaveform(const std::filesystem::path& path);
        // 다른 사운드 객체의 MiniAudio 리소스와 재생 상태를 가져온다.
        void MoveFrom(Sound& source);
    
    public:
        Sound();
        ~Sound();
    
        Sound(const Sound&) = delete;
        Sound& operator=(const Sound&) = delete;
        Sound(Sound&& other) noexcept;
        Sound& operator=(Sound&& other) noexcept;
        
        bool HasSound() const { return hasSound; }
        float GetVolume() const { return volume; }
        double GetLengthSeconds() const { return lengthSec; }
        const AudioWaveform& GetWaveform() const { return waveform; }

        // 볼륨을 유효 범위로 보정하고 재생 중인 사운드에 반영한다.
        void ApplyVolume(float value);
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
        // 사운드와 디코딩한 파형을 메모리에서 해제한다.
        void Unload() { UnInit(); }
    };
}
