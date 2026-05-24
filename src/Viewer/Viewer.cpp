#include "Viewer.h"

#include <windows.h>

#define	STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Chrivent {
    Viewer::Viewer() : info(std::make_unique<ViewerInfo>()) {}
    Viewer::~Viewer() = default;

    unsigned char* Viewer::LoadImageRgba(const std::filesystem::path& texturePath, int& x, int& y, int& comp) {
        x = y = comp = 0;
        FILE* imageFile = nullptr;
        if (_wfopen_s(&imageFile, texturePath.c_str(), L"rb") != 0 || !imageFile)
            return nullptr;
        stbi_uc* image = stbi_load_from_file(imageFile, &x, &y, &comp, STBI_rgb_alpha);
        std::fclose(imageFile);
        return image;
    }

    void Viewer::InitDirs(const std::filesystem::path& shaderSubDir) {
        std::vector<wchar_t> buf(MAX_PATH);
        while (true) {
            const DWORD n = GetModuleFileNameW(nullptr, buf.data(), buf.size());
            if (n < buf.size() - 1) {
                resourceDir = std::filesystem::path(std::wstring(buf.data(), n));
                break;
            }
            buf.resize(buf.size() * 2);
        }
        resourceDir = resourceDir.parent_path() / "resource";
        info->shaderDir = resourceDir / shaderSubDir;
        info->pmxDir = resourceDir / "mmd";
    }
}
