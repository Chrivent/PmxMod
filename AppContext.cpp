#include "AppContext.h"

#include <ranges>

#define	STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#include "base/Util.h"

AppContext::~AppContext() {
	Clear();
}

bool AppContext::Setup() {
	// Setup resource directory.
	m_resourceDir = PathUtil::GetExecutablePath();
	m_resourceDir = m_resourceDir.parent_path();
	m_resourceDir /= "resource";
	m_shaderDir = m_resourceDir / "shader";
	m_mmdDir = m_resourceDir / "mmd";

	m_mmdShader = std::make_unique<MMDShader>();
	if (!m_mmdShader->Setup(*this))
		return false;

	m_mmdEdgeShader = std::make_unique<MMDEdgeShader>();
	if (!m_mmdEdgeShader->Setup(*this))
		return false;

	m_mmdGroundShadowShader = std::make_unique<MMDGroundShadowShader>();
	if (!m_mmdGroundShadowShader->Setup(*this))
		return false;

	glGenTextures(1, &m_dummyColorTex);
	glBindTexture(GL_TEXTURE_2D, m_dummyColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &m_dummyShadowDepthTex);
	glBindTexture(GL_TEXTURE_2D, m_dummyShadowDepthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

void AppContext::Clear() {
	m_mmdShader.reset();
	m_mmdEdgeShader.reset();
	m_mmdGroundShadowShader.reset();

	for (auto &[m_texture, m_hasAlpha]: m_textures | std::views::values)
		glDeleteTextures(1, &m_texture);
	m_textures.clear();

	if (m_dummyColorTex != 0) glDeleteTextures(1, &m_dummyColorTex);
	if (m_dummyShadowDepthTex != 0) glDeleteTextures(1, &m_dummyShadowDepthTex);
	m_dummyColorTex = 0;
	m_dummyShadowDepthTex = 0;

	m_vmdCameraAnim.reset();
}

Texture AppContext::GetTexture(const std::filesystem::path& texturePath) {
	const auto it = m_textures.find(texturePath);
	if (it != m_textures.end())
		return it->second;
	stbi_set_flip_vertically_on_load(true);
	FILE* fp = nullptr;
	if (_wfopen_s(&fp, texturePath.c_str(), L"rb") != 0 || !fp)
		return Texture{ 0, false };
	int x = 0, y = 0, comp = 0;
	stbi_uc* image = stbi_load_from_file(fp, &x, &y, &comp, STBI_rgb_alpha);
	std::fclose(fp);
	if (!image)
		return Texture{ 0, false };
	const bool hasAlpha = comp == 4;
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	stbi_image_free(image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	m_textures[texturePath] = Texture{ tex, hasAlpha };
	return m_textures[texturePath];
}
