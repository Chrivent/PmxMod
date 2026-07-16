#include "Viewer/Texture/TextureCache.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Chrivent {
	void TextureImageLoader::ImageDeleter::operator()(unsigned char* pixels) const {
		stbi_image_free(pixels);
	}

	TextureImageLoader::LoadedImage TextureImageLoader::LoadImageRgba(const std::filesystem::path& texturePath) {
		LoadedImage image;
		FILE* imageFile = nullptr;
		if (_wfopen_s(&imageFile, texturePath.c_str(), L"rb") != 0 || imageFile == nullptr)
			return image;
		image.pixels.reset(stbi_load_from_file(
			imageFile, &image.width, &image.height, &image.components, STBI_rgb_alpha));
		std::fclose(imageFile);
		if (image.width <= 0 || image.height <= 0)
			image.pixels.reset();
		return image;
	}
}
