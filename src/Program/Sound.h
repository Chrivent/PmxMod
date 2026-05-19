#pragma once

#include <filesystem>

struct ma_engine;
struct ma_sound;

namespace Chrivent {
    class Sound {
        float volume = 0.1f;
        double lengthSec = 0.0;
        std::unique_ptr<ma_engine> engine;
        std::unique_ptr<ma_sound>  sound;
        double prevTimeSec = 0.0;
        bool playing = false;

        // miniaudio 엔진과 사운드 객체를 해제한다.
        void UnInit();
    
    public:
        bool hasSound = false;
        
        Sound();
        ~Sound();
    
        Sound(const Sound&) = delete;
        Sound& operator=(const Sound&) = delete;
        Sound(Sound&&) = delete;
        Sound& operator=(Sound&&) = delete;
        
        float GetVolume() const { return volume; }
        // 전체 사운드 길이를 초 단위로 반환한다.
        double GetLengthSeconds() const { return lengthSec; }

        // 오디오 파일을 열고 필요하면 반복 재생으로 준비한다.
        bool Init(const std::filesystem::path& path, bool loop);
        // 볼륨을 0.0~1.0 범위로 설정하고 MiniAudio 사운드 객체에 반영한다.
        void SetVolume(float value);
        // 이전 호출 시각과 현재 재생 시각을 출력하고 내부 기준 시간을 갱신한다.
        void PullTimes(float& deltaTime, float& time);
        // 재생 중인 사운드를 일시정지한다.
        void Pause();
        // 사운드 재생을 재개한다.
        void Resume();
        // 사운드 재생 위치를 지정한 초로 이동한다.
        void SeekSeconds(float seconds);
        // 사운드를 정지하고 재생 위치를 초기화한다.
        void Stop();
    };
}
