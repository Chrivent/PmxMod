#include "GLFWViewer.h"

#include "../src/MMDReader.h"
#include "../src/MMDUtil.h"
#include "../src/MMDModel.h"

#include "../external/stb_image.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <ranges>

GLuint CreateShader(const std::filesystem::path& file) {
	auto Compile = [](const GLenum shaderType, const std::string& code) -> GLuint {
		const GLuint shader = glCreateShader(shaderType);
		if (!shader) {
			std::cout << "Failed to create shader_GLFW.\n";
			return 0;
		}
		const char* codes = code.c_str();
		const auto codesLen = static_cast<GLint>(code.size());
		glShaderSource(shader, 1, &codes, &codesLen);
		glCompileShader(shader);
		GLint compileStatus = GL_FALSE, infoLength = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLength);
		if (infoLength > 1) {
			std::string log(static_cast<size_t>(infoLength), '\0');
			GLsizei len = 0;
			glGetShaderInfoLog(shader, infoLength, &len, log.data());
			std::cout << log.c_str() << "\n";
		}
		if (compileStatus != GL_TRUE) {
			glDeleteShader(shader);
			std::cout << "Failed to compile shader_GLFW.\n";
			return 0;
		}
		return shader;
	};
	std::ifstream f(file);
	if (!f) {
		std::cout << "Failed to open shader file. [" << file << "].\n";
		return 0;
	}
	auto InjectDefine = [](const std::string& src, const char* defineLine) {
		if (src.rfind("#version", 0) == 0) {
			const auto nl = src.find('\n');
			if (nl != std::string::npos) {
				std::string out;
				out.reserve(src.size() + 64);
				out.append(src, 0, nl + 1);
				out.append(defineLine);
				out.push_back('\n');
				out.append(src, nl + 1, std::string::npos);
				return out;
			}
		}
		return std::string(defineLine) + "\n" + src;
	};
	const std::string src((std::istreambuf_iterator(f)), {});
	const std::string vsCode = InjectDefine(src, "#define VERTEX");
	const std::string fsCode = InjectDefine(src, "#define FRAGMENT");
	const GLuint vs = Compile(GL_VERTEX_SHADER, vsCode);
	const GLuint fs = Compile(GL_FRAGMENT_SHADER, fsCode);
	if (!vs || !fs) {
		if (vs)
			glDeleteShader(vs);
		if (fs)
			glDeleteShader(fs);
		return 0;
	}
	const GLuint prog = glCreateProgram();
	if (prog == 0) {
		glDeleteShader(vs);
		glDeleteShader(fs);
		std::cout << "Failed to create program.\n";
		return 0;
	}
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	GLint linkStatus = GL_FALSE, infoLength = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &linkStatus);
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infoLength);
	if (infoLength > 1) {
		std::string info(static_cast<size_t>(infoLength), '\0');
		GLsizei len = 0;
		glGetProgramInfoLog(prog, infoLength, &len, info.data());
		std::cout << info.c_str() << "\n";
	}
	glDeleteShader(vs);
	glDeleteShader(fs);
	if (linkStatus != GL_TRUE) {
		glDeleteProgram(prog);
		std::cout << "Failed to link shader_GLFW.\n";
		return 0;
	}
	return prog;
}

GLFWShader::~GLFWShader() {
	if (m_prog != 0)
		glDeleteProgram(m_prog);
	m_prog = 0;
}

bool GLFWShader::Setup(const GLFWViewer& viewer) {
	m_prog = CreateShader(viewer.m_shaderDir / "mmd.glsl");
	if (m_prog == 0)
		return false;
	m_inPos = glGetAttribLocation(m_prog, "in_Pos");
	m_inNor = glGetAttribLocation(m_prog, "in_Nor");
	m_inUV = glGetAttribLocation(m_prog, "in_UV");
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

GLFWEdgeShader::~GLFWEdgeShader() {
	if (m_prog != 0)
		glDeleteProgram(m_prog);
	m_prog = 0;
}

bool GLFWEdgeShader::Setup(const GLFWViewer& viewer) {
	m_prog = CreateShader(viewer.m_shaderDir / "mmd_edge.glsl");
	if (m_prog == 0)
		return false;
	m_inPos = glGetAttribLocation(m_prog, "in_Pos");
	m_inNor = glGetAttribLocation(m_prog, "in_Nor");
	m_uWV = glGetUniformLocation(m_prog, "u_WV");
	m_uWVP = glGetUniformLocation(m_prog, "u_WVP");
	m_uScreenSize = glGetUniformLocation(m_prog, "u_ScreenSize");
	m_uEdgeSize = glGetUniformLocation(m_prog, "u_EdgeSize");
	m_uEdgeColor = glGetUniformLocation(m_prog, "u_EdgeColor");
	return true;
}

GLFWGroundShadowShader::~GLFWGroundShadowShader() {
	if (m_prog != 0) glDeleteProgram(m_prog);
	m_prog = 0;
}

bool GLFWGroundShadowShader::Setup(const GLFWViewer& viewer) {
	m_prog = CreateShader(viewer.m_shaderDir / "mmd_ground_shadow.glsl");
	if (m_prog == 0)
		return false;
	m_inPos = glGetAttribLocation(m_prog, "in_Pos");
	m_uWVP = glGetUniformLocation(m_prog, "u_WVP");
	m_uShadowColor = glGetUniformLocation(m_prog, "u_ShadowColor");
	return true;
}

GLFWMaterial::GLFWMaterial(const MMDMaterial &mat)
	: m_mmdMat(mat) {
}

bool GLFWModel::Setup(Viewer& viewer) {
	auto& glfwViewer = dynamic_cast<GLFWViewer&>(viewer);
	if (m_mmdModel == nullptr)
		return false;
	// Setup vertices
	const size_t vtxCount = m_mmdModel->m_positions.size();
	glGenBuffers(1, &m_posVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glGenBuffers(1, &m_norVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_norVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glGenBuffers(1, &m_uvVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_uvVBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(glm::vec2) * vtxCount), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	const size_t idxSize = m_mmdModel->m_indexElementSize;
	const size_t idxCount = m_mmdModel->m_indexCount;
	glGenBuffers(1, &m_ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idxSize * idxCount), &m_mmdModel->m_indices[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	if (idxSize == 1)
		m_indexType = GL_UNSIGNED_BYTE;
	else if (idxSize == 2)
		m_indexType = GL_UNSIGNED_SHORT;
	else if (idxSize == 4)
		m_indexType = GL_UNSIGNED_INT;
	else
		return false;
	// Setup MMD VAO
	glGenVertexArrays(1, &m_mmdVAO);
	glBindVertexArray(m_mmdVAO);
	const auto &mmdShader = glfwViewer.m_shader;
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glVertexAttribPointer(mmdShader->m_inPos, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
	glEnableVertexAttribArray(mmdShader->m_inPos);
	glBindBuffer(GL_ARRAY_BUFFER, m_norVBO);
	glVertexAttribPointer(mmdShader->m_inNor, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
	glEnableVertexAttribArray(mmdShader->m_inNor);
	glBindBuffer(GL_ARRAY_BUFFER, m_uvVBO);
	glVertexAttribPointer(mmdShader->m_inUV, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
	glEnableVertexAttribArray(mmdShader->m_inUV);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBindVertexArray(0);
	// Setup MMD Edge VAO
	glGenVertexArrays(1, &m_mmdEdgeVAO);
	glBindVertexArray(m_mmdEdgeVAO);
	const auto &mmdEdgeShader = glfwViewer.m_edgeShader;
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glVertexAttribPointer(mmdEdgeShader->m_inPos, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
	glEnableVertexAttribArray(mmdEdgeShader->m_inPos);
	glBindBuffer(GL_ARRAY_BUFFER, m_norVBO);
	glVertexAttribPointer(mmdEdgeShader->m_inNor, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
	glEnableVertexAttribArray(mmdEdgeShader->m_inNor);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBindVertexArray(0);
	// Setup MMD Ground Shadow VAO
	glGenVertexArrays(1, &m_mmdGroundShadowVAO);
	glBindVertexArray(m_mmdGroundShadowVAO);
	const auto &mmdGroundShadowShader = glfwViewer.m_groundShadowShader;
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glVertexAttribPointer(mmdGroundShadowShader->m_inPos, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
	glEnableVertexAttribArray(mmdGroundShadowShader->m_inPos);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBindVertexArray(0);
	// Setup materials
	for (const auto& mmdMat : m_mmdModel->m_materials) {
		GLFWMaterial mat(mmdMat);
		if (!mmdMat.m_texture.empty()) {
			auto [m_texture, m_hasAlpha] = glfwViewer.GetTexture(mmdMat.m_texture);
			mat.m_texture = m_texture;
			mat.m_textureHasAlpha = m_hasAlpha;
		}
		if (!mmdMat.m_spTexture.empty())
			mat.m_spTexture = glfwViewer.GetTexture(mmdMat.m_spTexture).m_texture;
		if (!mmdMat.m_toonTexture.empty())
			mat.m_toonTexture = glfwViewer.GetTexture(mmdMat.m_toonTexture).m_texture;
		m_materials.emplace_back(mat);
	}
	return true;
}

void GLFWModel::Clear() {
	if (m_posVBO != 0) glDeleteBuffers(1, &m_posVBO);
	if (m_norVBO != 0) glDeleteBuffers(1, &m_norVBO);
	if (m_uvVBO != 0) glDeleteBuffers(1, &m_uvVBO);
	if (m_ibo != 0) glDeleteBuffers(1, &m_ibo);
	m_posVBO = 0;
	m_norVBO = 0;
	m_uvVBO = 0;
	m_ibo = 0;
	if (m_mmdVAO != 0) glDeleteVertexArrays(1, &m_mmdVAO);
	if (m_mmdEdgeVAO != 0) glDeleteVertexArrays(1, &m_mmdEdgeVAO);
	if (m_mmdGroundShadowVAO != 0) glDeleteVertexArrays(1, &m_mmdGroundShadowVAO);
	m_mmdVAO = 0;
	m_mmdEdgeVAO = 0;
	m_mmdGroundShadowVAO = 0;
}

void GLFWModel::Update() const {
	m_mmdModel->Update();
	const size_t vtxCount = m_mmdModel->m_positions.size();
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount), m_mmdModel->m_updatePositions.data());
	glBindBuffer(GL_ARRAY_BUFFER, m_norVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount), m_mmdModel->m_updateNormals.data());
	glBindBuffer(GL_ARRAY_BUFFER, m_uvVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec2) * vtxCount), m_mmdModel->m_updateUVs.data());
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLFWModel::Draw(Viewer& viewer) const {
	auto& glfwViewer = dynamic_cast<GLFWViewer&>(viewer);
	const auto &view = viewer.m_viewMat;
	const auto &proj = viewer.m_projMat;
	auto world = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));
	auto wv = view * world;
	auto wvp = proj * view * world;
	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_2D, glfwViewer.m_dummyShadowDepthTex);
	glActiveTexture(GL_TEXTURE0 + 4);
	glBindTexture(GL_TEXTURE_2D, glfwViewer.m_dummyShadowDepthTex);
	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_2D, glfwViewer.m_dummyShadowDepthTex);
	glActiveTexture(GL_TEXTURE0 + 6);
	glBindTexture(GL_TEXTURE_2D, glfwViewer.m_dummyShadowDepthTex);
	glEnable(GL_DEPTH_TEST);
	// Draw model
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& shader = glfwViewer.m_shader;
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		if (mat.m_mmdMat.m_diffuse.a == 0)
			continue;
		glUseProgram(shader->m_prog);
		glBindVertexArray(m_mmdVAO);
		glUniformMatrix4fv(shader->m_uWV, 1, GL_FALSE, &wv[0][0]);
		glUniformMatrix4fv(shader->m_uWVP, 1, GL_FALSE, &wvp[0][0]);
		glUniform3fv(shader->m_uAmbient, 1, &mmdMat.m_ambient[0]);
		glUniform3fv(shader->m_uDiffuse, 1, &mmdMat.m_diffuse[0]);
		glUniform3fv(shader->m_uSpecular, 1, &mmdMat.m_specular[0]);
		glUniform1f(shader->m_uSpecularPower, mmdMat.m_specularPower);
		glUniform1f(shader->m_uAlpha, mmdMat.m_diffuse.a);
		glActiveTexture(GL_TEXTURE0 + 0);
		glUniform1i(shader->m_uTex, 0);
		if (mat.m_texture != 0) {
			if (!mat.m_textureHasAlpha)
				glUniform1i(shader->m_uTexMode, 1);
			else
				glUniform1i(shader->m_uTexMode, 2);
			glUniform4fv(shader->m_uTexMulFactor, 1, &mmdMat.m_textureMulFactor[0]);
			glUniform4fv(shader->m_uTexAddFactor, 1, &mmdMat.m_textureAddFactor[0]);
			glBindTexture(GL_TEXTURE_2D, mat.m_texture);
		} else {
			glUniform1i(shader->m_uTexMode, 0);
			glBindTexture(GL_TEXTURE_2D, glfwViewer.m_dummyColorTex);
		}
		glActiveTexture(GL_TEXTURE0 + 1);
		glUniform1i(shader->m_uSphereTex, 1);
		if (mat.m_spTexture != 0) {
			if (mmdMat.m_spTextureMode == SphereMode::Mul)
				glUniform1i(shader->m_uSphereTexMode, 1);
			else if (mmdMat.m_spTextureMode == SphereMode::Add)
				glUniform1i(shader->m_uSphereTexMode, 2);
			glUniform4fv(shader->m_uSphereTexMulFactor, 1, &mmdMat.m_spTextureMulFactor[0]);
			glUniform4fv(shader->m_uSphereTexAddFactor, 1, &mmdMat.m_spTextureAddFactor[0]);
			glBindTexture(GL_TEXTURE_2D, mat.m_spTexture);
		} else {
			glUniform1i(shader->m_uSphereTexMode, 0);
			glBindTexture(GL_TEXTURE_2D, glfwViewer.m_dummyColorTex);
		}
		glActiveTexture(GL_TEXTURE0 + 2);
		glUniform1i(shader->m_uToonTex, 2);
		if (mat.m_toonTexture != 0) {
			glUniform4fv(shader->m_uToonTexMulFactor, 1, &mmdMat.m_toonTextureMulFactor[0]);
			glUniform4fv(shader->m_uToonTexAddFactor, 1, &mmdMat.m_toonTextureAddFactor[0]);
			glUniform1i(shader->m_uToonTexMode, 1);
			glBindTexture(GL_TEXTURE_2D, mat.m_toonTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		} else {
			glUniform1i(shader->m_uToonTexMode, 0);
			glBindTexture(GL_TEXTURE_2D, glfwViewer.m_dummyColorTex);
		}
		glm::vec3 lightColor = viewer.m_lightColor;
		glm::vec3 lightDir = viewer.m_lightDir;
		auto viewMat = glm::mat3(viewer.m_viewMat);
		lightDir = viewMat * lightDir;
		glUniform3fv(shader->m_uLightDir, 1, &lightDir[0]);
		glUniform3fv(shader->m_uLightColor, 1, &lightColor[0]);
		if (mmdMat.m_bothFace)
			glDisable(GL_CULL_FACE);
		else {
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUniform1i(shader->m_uShadowMapEnabled, 0);
		glUniform1i(shader->m_uShadowMap0, 3);
		glUniform1i(shader->m_uShadowMap1, 4);
		glUniform1i(shader->m_uShadowMap2, 5);
		glUniform1i(shader->m_uShadowMap3, 6);
		size_t offset = m_beginIndex * m_mmdModel->m_indexElementSize;
		glDrawElements(GL_TRIANGLES, m_vertexCount, m_indexType, reinterpret_cast<GLvoid *>(offset));
		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}
	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0 + 4);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0 + 6);
	glBindTexture(GL_TEXTURE_2D, 0);
	// Draw edge
	glm::vec2 screenSize(viewer.m_screenWidth, viewer.m_screenHeight);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& shader = glfwViewer.m_edgeShader;
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		if (!mmdMat.m_edgeFlag)
			continue;
		if (mmdMat.m_diffuse.a == 0.0f)
			continue;
		glUseProgram(shader->m_prog);
		glBindVertexArray(m_mmdEdgeVAO);
		glUniformMatrix4fv(shader->m_uWV, 1, GL_FALSE, &wv[0][0]);
		glUniformMatrix4fv(shader->m_uWVP, 1, GL_FALSE, &wvp[0][0]);
		glUniform2fv(shader->m_uScreenSize, 1, &screenSize[0]);
		glUniform1f(shader->m_uEdgeSize, mmdMat.m_edgeSize);
		glUniform4fv(shader->m_uEdgeColor, 1, &mmdMat.m_edgeColor[0]);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		size_t offset = m_beginIndex * m_mmdModel->m_indexElementSize;
		glDrawElements(GL_TRIANGLES, m_vertexCount, m_indexType, reinterpret_cast<GLvoid *>(offset));
		glBindVertexArray(0);
		glUseProgram(0);
	}
	// Draw ground shadow
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1, -1);
	auto plane = glm::vec4(0, 1, 0, 0);
	auto light = -viewer.m_lightDir;
	auto shadow = glm::mat4(1);
	shadow[0][0] = plane.y * light.y + plane.z * light.z;
	shadow[0][1] = -plane.x * light.y;
	shadow[0][2] = -plane.x * light.z;
	shadow[0][3] = 0;
	shadow[1][0] = -plane.y * light.x;
	shadow[1][1] = plane.x * light.x + plane.z * light.z;
	shadow[1][2] = -plane.y * light.z;
	shadow[1][3] = 0;
	shadow[2][0] = -plane.z * light.x;
	shadow[2][1] = -plane.z * light.y;
	shadow[2][2] = plane.x * light.x + plane.y * light.y;
	shadow[2][3] = 0;
	shadow[3][0] = -plane.w * light.x;
	shadow[3][1] = -plane.w * light.y;
	shadow[3][2] = -plane.w * light.z;
	shadow[3][3] = plane.x * light.x + plane.y * light.y + plane.z * light.z;
	auto shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
	if (shadowColor.a < 1.0f) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glStencilFuncSeparate(GL_FRONT_AND_BACK, GL_NOTEQUAL, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glEnable(GL_STENCIL_TEST);
	} else
		glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		const auto& shader = glfwViewer.m_groundShadowShader;
		if (!mmdMat.m_groundShadow)
			continue;
		if (mmdMat.m_diffuse.a == 0.0f)
			continue;
		glUseProgram(shader->m_prog);
		glBindVertexArray(m_mmdGroundShadowVAO);
		glUniformMatrix4fv(shader->m_uWVP, 1, GL_FALSE, &(proj * view * shadow * world)[0][0]);
		glUniform4fv(shader->m_uShadowColor, 1, &shadowColor[0]);
		size_t offset = m_beginIndex * m_mmdModel->m_indexElementSize;
		glDrawElements(GL_TRIANGLES, m_vertexCount, m_indexType, reinterpret_cast<GLvoid *>(offset));
		glBindVertexArray(0);
		glUseProgram(0);
	}
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
}

GLFWViewer::~GLFWViewer() {
	m_shader.reset();
	m_edgeShader.reset();
	m_groundShadowShader.reset();
	for (auto &[m_texture, m_hasAlpha]: m_textures | std::views::values)
		glDeleteTextures(1, &m_texture);
	m_textures.clear();
	if (m_dummyColorTex != 0)
		glDeleteTextures(1, &m_dummyColorTex);
	if (m_dummyShadowDepthTex != 0)
		glDeleteTextures(1, &m_dummyShadowDepthTex);
	m_dummyColorTex = 0;
	m_dummyShadowDepthTex = 0;
	m_vmdCameraAnim.reset();
}

bool GLFWViewer::Run(const SceneConfig& cfg) {
	MusicUtil music;
	music.Init(cfg.musicPath);
	if (!glfwInit())
		return false;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, m_msaaSamples);
	GLFWwindow* window = glfwCreateWindow(1920, 1080, "Pmx Mod", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		glfwTerminate();
		return false;
	}
	glfwSwapInterval(0);
	glEnable(GL_MULTISAMPLE);
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	if (width <= 0 || height <= 0) {
		glfwTerminate();
		return false;
	}
	if (!Setup()) {
		std::cout << "Failed to setup Viewer.\n";
		glfwTerminate();
		return false;
	}
	LoadCameraVmd(cfg);
	std::vector<std::unique_ptr<Model>> models;
	if (!LoadModels(cfg, models)) {
		glfwTerminate();
		return false;
	}
	auto fpsTime  = std::chrono::steady_clock::now();
	auto saveTime = std::chrono::steady_clock::now();
	int fpsFrame  = 0;
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		int newW = 0, newH = 0;
		glfwGetFramebufferSize(window, &newW, &newH);
		if (newW != width || newH != height) {
			width = newW;
			height = newH;
			glViewport(0, 0, width, height);
		}
		StepTime(music, saveTime);
		UpdateCamera(width, height);
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		for (const auto& model : models) {
			model->UpdateAnimation(*this);
			model->Update();
			model->Draw(*this);
		}
		glfwSwapBuffers(window);
		TickFps(fpsTime, fpsFrame);
	}
	for (const auto& model : models)
		model->Clear();
	models.clear();
	glfwTerminate();
	return true;
}

std::unique_ptr<Model> GLFWViewer::CreateModel() const {
	return std::make_unique<GLFWModel>();
}

bool GLFWViewer::Setup() {
	InitDirs("shader_GLFW");
	m_shader = std::make_unique<GLFWShader>();
	if (!m_shader->Setup(*this))
		return false;
	m_edgeShader = std::make_unique<GLFWEdgeShader>();
	if (!m_edgeShader->Setup(*this))
		return false;
	m_groundShadowShader = std::make_unique<GLFWGroundShadowShader>();
	if (!m_groundShadowShader->Setup(*this))
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

GLFWTexture GLFWViewer::GetTexture(const std::filesystem::path& texturePath) {
	const auto it = m_textures.find(texturePath);
	if (it != m_textures.end())
		return it->second;
	int x = 0, y = 0, comp = 0;
	stbi_set_flip_vertically_on_load(true);
	stbi_uc* image = LoadImageRGBA(texturePath, x, y, comp);
	stbi_set_flip_vertically_on_load(false);
	if (!image)
		return GLFWTexture{ 0, false };
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
	m_textures[texturePath] = GLFWTexture{ tex, hasAlpha };
	return m_textures[texturePath];
}
