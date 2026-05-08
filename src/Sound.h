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
    double m_lengthSec = 0.0;

    /// 오디오 파일을 열고 필요하면 반복 재생으로 준비한다.
    bool Init(const std::filesystem::path& path, bool loop);
    /// 현재 볼륨 값을 miniaudio 사운드 객체에 반영한다.
    void ApplyVolume();
    /// 이전 호출 시각과 현재 재생 시각을 반환하고 내부 기준 시간을 갱신한다.
    std::pair<float, float> PullTimes();
    /// 재생 중인 사운드를 일시정지한다.
    void Pause() const;
    /// 사운드 재생을 재개한다.
    void Resume();
    /// 사운드를 정지하고 재생 위치를 초기화한다.
    void Stop();

private:
    std::unique_ptr<ma_engine> m_engine;
    std::unique_ptr<ma_sound>  m_sound;
    double m_prevTimeSec = 0.0;

    /// miniaudio 엔진과 사운드 객체를 해제한다.
    void Uninit();
};
