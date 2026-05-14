#include "Viewer.h"

#include <iostream>
#include <windows.h>

#define	STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Chrivent {
    Viewer::~Viewer() = default;

    unsigned char* Viewer::LoadImageRgba(const std::filesystem::path& texturePath, int& x, int& y, int& comp, const bool flipY) {
        stbi_set_flip_vertically_on_load(flipY);
        x = y = comp = 0;
        FILE* imageFile = nullptr;
        if (_wfopen_s(&imageFile, texturePath.c_str(), L"rb") != 0 || !imageFile)
            return nullptr;
        stbi_uc* image = stbi_load_from_file(imageFile, &x, &y, &comp, STBI_rgb_alpha);
        std::fclose(imageFile);
        return image;
    }

    bool Viewer::LoadInstances(const SceneConfig& cfg, std::vector<std::unique_ptr<Instance>>& instances) {
        instances.clear();
        instances.reserve(cfg.modelConfigs.size());
        for (const auto& [modelPath, animPaths, scale] : cfg.modelConfigs) {
            auto instance = CreateInstance();
            const auto pmxModel = std::make_shared<Model>();
            if (!pmxModel->Load(modelPath, pmxDir)) {
                std::cout << "Failed to load pmx file.\n";
                return false;
            }
            instance->model = pmxModel;
            instance->model->InitializeAnimation();
            auto vmdAnim = std::make_unique<Animation>();
            vmdAnim->model = instance->model;
            for (const auto& vmdPath : animPaths) {
                VmdReader vmd;
                if (!vmd.ReadFile(vmdPath.c_str())) {
                    std::cout << "Failed to read VMD file.\n";
                    return false;
                }
                if (!vmdAnim->Add(vmd)) {
                    std::cout << "Failed to add VMDAnimation.\n";
                    return false;
                }
            }
            vmdAnim->SyncPhysics(0.0f);
            instance->anim = std::move(vmdAnim);
            instance->scale = scale;
            if (!instance->Setup(*this))
                return false;
            instances.emplace_back(std::move(instance));
        }
        return true;
    }

    void Viewer::InitDirs(const std::filesystem::path& shaderSubDir) {
        std::vector<wchar_t> buf(MAX_PATH);
        while (true) {
            const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
            if (n < buf.size() - 1) {
                resourceDir = std::filesystem::path(std::wstring(buf.data(), n));
                break;
            }
            buf.resize(buf.size() * 2);
        }
        resourceDir = resourceDir.parent_path() / "resource";
        shaderDir = resourceDir / shaderSubDir;
        pmxDir = resourceDir / "mmd";
    }
}
