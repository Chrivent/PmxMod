#include "MMDShader.h"

#include <iostream>
#include <vector>

#include "AppContext.h"

#include "File.h"
#include "Path.h"

GLuint CreateShader(const GLenum shaderType, const std::string &code)
{
	const GLuint shader = glCreateShader(shaderType);
	if (shader == 0)
	{
		std::cout << "Failed to create shader.\n";
		return 0;
	}
	const char* codes[] = { code.c_str() };
	GLint codesLen[] = { static_cast<GLint>(code.size()) };
	glShaderSource(shader, 1, codes, codesLen);
	glCompileShader(shader);

	GLint infoLength;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLength);
	if (infoLength != 0)
	{
		std::vector<char> info;
		info.reserve(infoLength + 1);
		info.resize(infoLength);

		GLsizei len;
		glGetShaderInfoLog(shader, infoLength, &len, &info[0]);
		if (info[infoLength - 1] != '\0')
		{
			info.push_back('\0');
		}

		std::cout << &info[0] << "\n";
	}

	GLint compileStatus;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus != GL_TRUE)
	{
		glDeleteShader(shader);
		std::cout << "Failed to compile shader.\n";
		return 0;
	}

	return shader;
}

GLuint CreateShaderProgram(const std::string vsFile, const std::string fsFile)
{
	saba::File vsFileText;
	if (!vsFileText.OpenFile(vsFile.c_str(), "r"))
	{
		std::cout << "Failed to open shader file. [" << vsFile << "].\n";
		return 0;
	}
	const std::string vsCode = vsFileText.ReadAll();
	vsFileText.Close();

	saba::File fsFileText;
	if (!fsFileText.OpenFile(fsFile.c_str(), "r"))
	{
		std::cout << "Failed to open shader file. [" << fsFile << "].\n";
		return 0;
	}
	const std::string fsCode = fsFileText.ReadAll();
	fsFileText.Close();

	const GLuint vs = CreateShader(GL_VERTEX_SHADER, vsCode);
	const GLuint fs = CreateShader(GL_FRAGMENT_SHADER, fsCode);
	if (vs == 0 || fs == 0)
	{
		if (vs != 0) { glDeleteShader(vs); }
		if (fs != 0) { glDeleteShader(fs); }
		return 0;
	}

	const GLuint prog = glCreateProgram();
	if (prog == 0)
	{
		glDeleteShader(vs);
		glDeleteShader(fs);
		std::cout << "Failed to create program.\n";
		return 0;
	}
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);

	GLint infoLength;
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infoLength);
	if (infoLength != 0)
	{
		std::vector<char> info;
		info.reserve(infoLength + 1);
		info.resize(infoLength);

		GLsizei len;
		glGetProgramInfoLog(prog, infoLength, &len, &info[0]);
		if (info[infoLength - 1] != '\0')
		{
			info.push_back('\0');
		}

		std::cout << &info[0] << "\n";
	}

	GLint linkStatus;
	glGetProgramiv(prog, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE)
	{
		glDeleteShader(vs);
		glDeleteShader(fs);
		std::cout << "Failed to link shader.\n";
		return 0;
	}

	glDeleteShader(vs);
	glDeleteShader(fs);
	return prog;
}

MMDShader::~MMDShader()
{
	Clear();
}

bool MMDShader::Setup(const AppContext& appContext)
{
	m_prog = CreateShaderProgram(
		saba::PathUtil::Combine(appContext.m_shaderDir, "mmd.vert"),
		saba::PathUtil::Combine(appContext.m_shaderDir, "mmd.frag")
	);
	if (m_prog == 0)
	{
		return false;
	}

	// attribute
	m_inPos = glGetAttribLocation(m_prog, "in_Pos");
	m_inNor = glGetAttribLocation(m_prog, "in_Nor");
	m_inUV = glGetAttribLocation(m_prog, "in_UV");

	// uniform
	m_uWV = glGetUniformLocation(m_prog, "u_WV");
	m_uWVP = glGetUniformLocation(m_prog, "u_WVP");

	m_uAmbient = glGetUniformLocation(m_prog, "u_Ambient");
	m_uDiffuse = glGetUniformLocation(m_prog, "u_Diffuse");
	m_uSpecular = glGetUniformLocation(m_prog, "u_Specular");
	m_uSpecularPower = glGetUniformLocation(m_prog, "u_SpecularPower");
	m_uAlpha = glGetUniformLocation(m_prog, "u_Alpha");

	m_uTexMode = glGetUniformLocation(m_prog, "u_TexMode");
	m_uTex = glGetUniformLocation(m_prog, "u_Tex");
	m_uTexMulFactor = glGetUniformLocation(m_prog, "u_TexMulFactor");
	m_uTexAddFactor = glGetUniformLocation(m_prog, "u_TexAddFactor");

	m_uSphereTexMode = glGetUniformLocation(m_prog, "u_SphereTexMode");
	m_uSphereTex = glGetUniformLocation(m_prog, "u_SphereTex");
	m_uSphereTexMulFactor = glGetUniformLocation(m_prog, "u_SphereTexMulFactor");
	m_uSphereTexAddFactor = glGetUniformLocation(m_prog, "u_SphereTexAddFactor");

	m_uToonTexMode = glGetUniformLocation(m_prog, "u_ToonTexMode");
	m_uToonTex = glGetUniformLocation(m_prog, "u_ToonTex");
	m_uToonTexMulFactor = glGetUniformLocation(m_prog, "u_ToonTexMulFactor");
	m_uToonTexAddFactor = glGetUniformLocation(m_prog, "u_ToonTexAddFactor");

	m_uLightColor = glGetUniformLocation(m_prog, "u_LightColor");
	m_uLightDir = glGetUniformLocation(m_prog, "u_LightDir");

	m_uLightVP = glGetUniformLocation(m_prog, "u_LightWVP");
	m_uShadowMapSplitPositions = glGetUniformLocation(m_prog, "u_ShadowMapSplitPositions");
	m_uShadowMap0 = glGetUniformLocation(m_prog, "u_ShadowMap0");
	m_uShadowMap1 = glGetUniformLocation(m_prog, "u_ShadowMap1");
	m_uShadowMap2 = glGetUniformLocation(m_prog, "u_ShadowMap2");
	m_uShadowMap3 = glGetUniformLocation(m_prog, "u_ShadowMap3");
	m_uShadowMapEnabled = glGetUniformLocation(m_prog, "u_ShadowMapEnabled");

	return true;
}

void MMDShader::Clear()
{
	if (m_prog != 0) { glDeleteProgram(m_prog); }
	m_prog = 0;
}

/*
	MMDEdgeShader
*/

MMDEdgeShader::~MMDEdgeShader()
{
	Clear();
}

bool MMDEdgeShader::Setup(const AppContext& appContext)
{
	m_prog = CreateShaderProgram(
		saba::PathUtil::Combine(appContext.m_shaderDir, "mmd_edge.vert"),
		saba::PathUtil::Combine(appContext.m_shaderDir, "mmd_edge.frag")
	);
	if (m_prog == 0)
	{
		return false;
	}

	// attribute
	m_inPos = glGetAttribLocation(m_prog, "in_Pos");
	m_inNor = glGetAttribLocation(m_prog, "in_Nor");

	// uniform
	m_uWV = glGetUniformLocation(m_prog, "u_WV");
	m_uWVP = glGetUniformLocation(m_prog, "u_WVP");
	m_uScreenSize = glGetUniformLocation(m_prog, "u_ScreenSize");
	m_uEdgeSize = glGetUniformLocation(m_prog, "u_EdgeSize");
	m_uEdgeColor = glGetUniformLocation(m_prog, "u_EdgeColor");

	return true;
}

void MMDEdgeShader::Clear()
{
	if (m_prog != 0) { glDeleteProgram(m_prog); }
	m_prog = 0;
}

/*
	MMDGroundShadowShader
*/

MMDGroundShadowShader::~MMDGroundShadowShader()
{
	Clear();
}

bool MMDGroundShadowShader::Setup(const AppContext& appContext)
{
	m_prog = CreateShaderProgram(
		saba::PathUtil::Combine(appContext.m_shaderDir, "mmd_ground_shadow.vert"),
		saba::PathUtil::Combine(appContext.m_shaderDir, "mmd_ground_shadow.frag")
	);
	if (m_prog == 0)
	{
		return false;
	}

	// attribute
	m_inPos = glGetAttribLocation(m_prog, "in_Pos");

	// uniform
	m_uWVP = glGetUniformLocation(m_prog, "u_WVP");
	m_uShadowColor = glGetUniformLocation(m_prog, "u_ShadowColor");

	return true;
}

void MMDGroundShadowShader::Clear()
{
	if (m_prog != 0) { glDeleteProgram(m_prog); }
	m_prog = 0;
}
