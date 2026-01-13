#include "MMDUtil.h"

#define MINIAUDIO_IMPLEMENTATION
#include "../external/miniaudio.h"

std::string UnicodeUtil::WStringToUtf8(const std::wstring& w) {
    if (w.empty())
        return {};
    const int need = WideCharToMultiByte(
        CP_UTF8, 0,
        w.data(), static_cast<int>(w.size()),
        nullptr, 0, nullptr, nullptr);
    if (need <= 0)
        return {};
    std::string utf8(static_cast<size_t>(need), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8, 0,
        w.data(), static_cast<int>(w.size()),
        utf8.data(), need, nullptr, nullptr);
    if (written != need)
        return {};
    return utf8;
}

std::string UnicodeUtil::SjisToUtf8(const char* sjis) {
    if (!sjis)
        return {};
    const int need = MultiByteToWideChar(
        932, MB_ERR_INVALID_CHARS,
        sjis, -1,
        nullptr, 0);
    if (need <= 0)
        return {};
    std::wstring w(static_cast<size_t>(need), L'\0');
    const int written = MultiByteToWideChar(
        932, MB_ERR_INVALID_CHARS,
        sjis, -1,
        w.data(), need);
    if (written <= 0)
        return {};
    if (!w.empty() && w.back() == L'\0')
        w.pop_back();
    return WStringToUtf8(w);
}

std::filesystem::path PathUtil::GetExecutablePath() {
    std::vector<wchar_t> buf(MAX_PATH);
    while (true) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0)
            return {};
        if (n < buf.size() - 1)
            return { buf.data(), buf.data() + n };
        buf.resize(buf.size() * 2);
    }
}

std::string PathUtil::GetExt(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.')
        ext.erase(ext.begin());
    for (auto& ch : ext)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return ext;
}

MusicUtil::MusicUtil() {
    m_engine = std::make_unique<ma_engine>();
    m_sound = std::make_unique<ma_sound>();
}

MusicUtil::~MusicUtil() {
    Uninit();
}

bool MusicUtil::Init(const std::filesystem::path& path) {
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
    ma_sound_start(m_sound.get());
    m_hasMusic = true;
    m_prevTimeSec = 0.0;
    return true;
}

bool MusicUtil::HasMusic() const {
    return m_hasMusic;
}

std::pair<float, float> MusicUtil::PullTimes() {
    if (!m_hasMusic)
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

void MusicUtil::Uninit() {
    if (!m_hasMusic)
        return;
    ma_sound_uninit(m_sound.get());
    ma_engine_uninit(m_engine.get());
    m_hasMusic = false;
    m_prevTimeSec = 0.0;
    m_engine.reset();
    m_sound.reset();
}
