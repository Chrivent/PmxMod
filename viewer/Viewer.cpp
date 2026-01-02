#include "Viewer.h"

#define	STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

unsigned char* AppContext::LoadImageRGBA(const std::filesystem::path& texturePath, int& x, int& y, int& comp) {
    stbi_uc* image = nullptr;
    x = y = comp = 0;
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, texturePath.c_str(), L"rb") != 0 || !fp)
        return nullptr;
    image = stbi_load_from_file(fp, &x, &y, &comp, STBI_rgb_alpha);
    std::fclose(fp);
    return image;
}
