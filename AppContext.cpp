#include "AppContext.h"

#include <iostream>

#define	STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"

#include "Path.h"
#include "MMDShader.h"

AppContext::~AppContext()
{
	Clear();
}

bool AppContext::Setup()
{
	// Setup resource directory.
	m_resourceDir = saba::PathUtil::GetExecutablePath();
	m_resourceDir = saba::PathUtil::GetDirectoryName(m_resourceDir);
	m_resourceDir = saba::PathUtil::Combine(m_resourceDir, "resource");
	m_shaderDir = saba::PathUtil::Combine(m_resourceDir, "shader");
	m_mmdDir = saba::PathUtil::Combine(m_resourceDir, "mmd");

	m_mmdShader = std::make_unique<MMDShader>();
	if (!m_mmdShader->Setup(*this))
	{
		return false;
	}

	m_mmdEdgeShader = std::make_unique<MMDEdgeShader>();
	if (!m_mmdEdgeShader->Setup(*this))
	{
		return false;
	}

	m_mmdGroundShadowShader = std::make_unique<MMDGroundShadowShader>();
	if (!m_mmdGroundShadowShader->Setup(*this))
	{
		return false;
	}

	glGenTextures(1, &m_dummyColorTex);
	glBindTexture(GL_TEXTURE_2D, m_dummyColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &m_dummyShadowDepthTex);
	glBindTexture(GL_TEXTURE_2D, m_dummyShadowDepthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Create Copy Transparent Window Shader (only windows)
	m_copyTransparentWindowShader = CreateShaderProgram(
		saba::PathUtil::Combine(m_shaderDir, "quad.vert"),
		saba::PathUtil::Combine(m_shaderDir, "copy_transparent_window.frag")
	);

	m_copyTransparentWindowShaderTex = glGetUniformLocation(m_copyTransparentWindowShader, "u_Tex");

	// Copy Shader
	m_copyShader = CreateShaderProgram(
		saba::PathUtil::Combine(m_shaderDir, "quad.vert"),
		saba::PathUtil::Combine(m_shaderDir, "copy.frag")
	);

	m_copyShaderTex = glGetUniformLocation(m_copyShader, "u_Tex");

	glGenVertexArrays(1, &m_copyVAO);

	return true;
}

void AppContext::Clear()
{
	m_mmdShader.reset();
	m_mmdEdgeShader.reset();
	m_mmdGroundShadowShader.reset();

	for (auto& tex : m_textures)
	{
		glDeleteTextures(1, &tex.second.m_texture);
	}
	m_textures.clear();

	if (m_dummyColorTex != 0) { glDeleteTextures(1, &m_dummyColorTex); }
	if (m_dummyShadowDepthTex != 0) { glDeleteTextures(1, &m_dummyShadowDepthTex); }
	m_dummyColorTex = 0;
	m_dummyShadowDepthTex = 0;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (m_transparentFbo != 0) { glDeleteFramebuffers(1, &m_transparentFbo); }
	if (m_transparentMSAAFbo != 0) { glDeleteFramebuffers(1, &m_transparentMSAAFbo); }
	if (m_transparentFboColorTex != 0) { glDeleteTextures(1, &m_transparentFboColorTex); }
	if (m_transparentFboMSAAColorRB != 0) { glDeleteRenderbuffers(1, &m_transparentFboMSAAColorRB); }
	if (m_transparentFboMSAADepthRB != 0) { glDeleteRenderbuffers(1, &m_transparentFboMSAADepthRB); }
	if (m_copyTransparentWindowShader != 0) { glDeleteProgram(m_copyTransparentWindowShader); }
	if (m_copyShader != 0) { glDeleteProgram(m_copyShader); }
	if (m_copyVAO != 0) { glDeleteVertexArrays(1, &m_copyVAO); }

	m_vmdCameraAnim.reset();
}

void AppContext::SetupTransparentFBO()
{
	// Setup FBO
	if (m_transparentFbo == 0)
	{
		glGenFramebuffers(1, &m_transparentFbo);
		glGenFramebuffers(1, &m_transparentMSAAFbo);
		glGenTextures(1, &m_transparentFboColorTex);
		glGenRenderbuffers(1, &m_transparentFboMSAAColorRB);
		glGenRenderbuffers(1, &m_transparentFboMSAADepthRB);
	}

	if ((m_screenWidth != m_transparentFboWidth) || (m_screenHeight != m_transparentFboHeight))
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, m_transparentFboColorTex);
		glTexImage2D(
			GL_TEXTURE_2D, 0, GL_RGBA,
			m_screenWidth,
			m_screenHeight,
			0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
		);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, m_transparentFbo);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_transparentFboColorTex, 0);
		if (GL_FRAMEBUFFER_COMPLETE != glCheckFramebufferStatus(GL_FRAMEBUFFER))
		{
			std::cout << "Faile to bind framebuffer.\n";
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glBindRenderbuffer(GL_RENDERBUFFER, m_transparentFboMSAAColorRB);
		//glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_screenWidth, m_screenHeight);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaaSamples, GL_RGBA, m_screenWidth, m_screenHeight);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		glBindRenderbuffer(GL_RENDERBUFFER, m_transparentFboMSAADepthRB);
		//glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_screenWidth, m_screenHeight);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaaSamples, GL_DEPTH24_STENCIL8, m_screenWidth, m_screenHeight);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, m_transparentMSAAFbo);
		//glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_transparentFboColorTex, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_transparentFboMSAAColorRB);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_transparentFboMSAADepthRB);
		auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (GL_FRAMEBUFFER_COMPLETE != status)
		{
			std::cout << "Faile to bind framebuffer.\n";
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		m_transparentFboWidth = m_screenWidth;
		m_transparentFboHeight = m_screenHeight;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, m_transparentMSAAFbo);
	glEnable(GL_MULTISAMPLE);
}

Texture AppContext::GetTexture(const std::string& texturePath)
{
	auto it = m_textures.find(texturePath);
	if (it == m_textures.end())
	{
		stbi_set_flip_vertically_on_load(true);
		saba::File file;
		if (!file.Open(texturePath))
		{
			return Texture{ 0, false };
		}
		int x, y, comp;
		int ret = stbi_info_from_file(file.GetFilePointer(), &x, &y, &comp);
		if (ret == 0)
		{
			return Texture{ 0, false };
		}

		GLuint tex;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);

		int reqComp = 0;
		bool hasAlpha = false;
		if (comp != 4)
		{
			uint8_t* image = stbi_load_from_file(file.GetFilePointer(), &x, &y, &comp, STBI_rgb);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
			stbi_image_free(image);
			hasAlpha = false;
		}
		else
		{
			uint8_t* image = stbi_load_from_file(file.GetFilePointer(), &x, &y, &comp, STBI_rgb_alpha);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
			stbi_image_free(image);
			hasAlpha = true;
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glBindTexture(GL_TEXTURE_2D, 0);

		m_textures[texturePath] = Texture{ tex, hasAlpha };

		return m_textures[texturePath];
	}
	else
	{
		return (*it).second;
	}
}
