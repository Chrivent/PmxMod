#include "GLFWViewer.h"

#include "../src/Reader.h"
#include "../src/Util.h"
#include "../src/Model.h"

#include "../external/stb_image.h"

#include <iostream>
#include <fstream>
#include <ranges>

GLuint CompileShader(const GLenum shaderType, const std::string& code) {
	const GLuint shader = glCreateShader(shaderType);
	if (!shader)
		return 0;
	const char* codes = code.c_str();
	const auto codesLen = static_cast<GLint>(code.size());
	glShaderSource(shader, 1, &codes, &codesLen);
	glCompileShader(shader);
	return shader;
}

GLuint CreateBuffer(const GLenum target, const size_t size, const void* data, const GLenum usage) {
	GLuint b = 0;
	glGenBuffers(1, &b);
	glBindBuffer(target, b);
	glBufferData(target, static_cast<GLsizeiptr>(size), data, usage);
	return b;
}

GLuint CreateVAO(const GLuint* buffers, const GLint* locs, const GLint* sizes, const GLenum* types,
	const int attribCount, const GLuint ibo) {
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	for (int i = 0; i < attribCount; i++) {
		if (locs[i] < 0)
			continue;
		glBindBuffer(GL_ARRAY_BUFFER, buffers[i]);
		glVertexAttribPointer(locs[i], sizes[i], types[i], GL_FALSE, 0, nullptr);
		glEnableVertexAttribArray(locs[i]);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBindVertexArray(0);
	return vao;
}

void GetUniforms(const GLuint prog, const char* const* names, GLint* const* outs, const int count) {
	for (int i = 0; i < count; i++)
		*outs[i] = glGetUniformLocation(prog, names[i]);
}

std::string InjectDefine(const std::string& src, const char* defineLine) {
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
}

GLuint CreateShader(const std::filesystem::path& file) {
	std::ifstream f(file);
	if (!f)
		return 0;
	const std::string src((std::istreambuf_iterator(f)), {});
	const std::string vsCode = InjectDefine(src, "#define VERTEX");
	const std::string fsCode = InjectDefine(src, "#define FRAGMENT");
	const GLuint vs = CompileShader(GL_VERTEX_SHADER, vsCode);
	const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsCode);
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
		return 0;
	}
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);
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
	m_inUV  = glGetAttribLocation(m_prog, "in_UV");
	const char* names[] = {
		"u_WV", "u_WVP",
		"u_Ambient", "u_Diffuse", "u_Specular", "u_SpecularPower", "u_Alpha",
		"u_TexMode", "u_Tex", "u_TexMulFactor", "u_TexAddFactor",
		"u_SphereTexMode", "u_SphereTex", "u_SphereTexMulFactor", "u_SphereTexAddFactor",
		"u_ToonTexMode", "u_ToonTex", "u_ToonTexMulFactor", "u_ToonTexAddFactor",
		"u_LightColor", "u_LightDir"
	};
	GLint* outs[] = {
		&m_uWV, &m_uWVP,
		&m_uAmbient, &m_uDiffuse, &m_uSpecular, &m_uSpecularPower, &m_uAlpha,
		&m_uTexMode, &m_uTex, &m_uTexMulFactor, &m_uTexAddFactor,
		&m_uSphereTexMode, &m_uSphereTex, &m_uSphereTexMulFactor, &m_uSphereTexAddFactor,
		&m_uToonTexMode, &m_uToonTex, &m_uToonTexMulFactor, &m_uToonTexAddFactor,
		&m_uLightColor, &m_uLightDir
	};
	GetUniforms(m_prog, names, outs, std::size(names));
	glUseProgram(m_prog);
	glUniform1i(m_uTex, 0);
	glUniform1i(m_uSphereTex, 1);
	glUniform1i(m_uToonTex, 2);
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
	if (m_prog != 0)
		glDeleteProgram(m_prog);
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

GLFWMaterial::GLFWMaterial(const Material &mat)
	: m_mmdMat(mat) {
}

bool GLFWInstance::Setup(Viewer& viewer) {
	m_viewer = &dynamic_cast<GLFWViewer&>(viewer);
	if (m_mmdModel == nullptr)
		return false;
	const size_t vtxCount = m_mmdModel->m_positions.size();
	m_posVBO = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
	m_norVBO = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
	m_uvVBO  = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec2) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
	const size_t idxSize = m_mmdModel->m_indexElementSize;
	const size_t idxCount = m_mmdModel->m_indexCount;
	m_ibo = CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, idxSize * idxCount, m_mmdModel->m_indices.data(), GL_STATIC_DRAW);
	if (idxSize == 1)
		m_indexType = GL_UNSIGNED_BYTE;
	else if (idxSize == 2)
		m_indexType = GL_UNSIGNED_SHORT;
	else if (idxSize == 4)
		m_indexType = GL_UNSIGNED_INT;
	else
		return false;
	const GLuint buffers[][3]   = {
		{ m_posVBO, m_norVBO, m_uvVBO },
		{ m_posVBO, m_norVBO },
		{ m_posVBO }
	};
	const GLint locs[][3] = {
		{ m_viewer->m_shader->m_inPos, m_viewer->m_shader->m_inNor, m_viewer->m_shader->m_inUV },
		{ m_viewer->m_edgeShader->m_inPos, m_viewer->m_edgeShader->m_inNor },
		{ m_viewer->m_groundShadowShader->m_inPos }
	};
	constexpr GLint sizes[][3] = {
		{ 3, 3, 2 },
		{ 3, 3 },
		{ 3 }
	};
	constexpr GLenum types[][3]  = {
		{ GL_FLOAT, GL_FLOAT, GL_FLOAT },
		{ GL_FLOAT, GL_FLOAT },
		{ GL_FLOAT }
	};
	m_mmdVAO = CreateVAO(buffers[0], locs[0], sizes[0], types[0], 3, m_ibo);
	m_mmdEdgeVAO = CreateVAO(buffers[1], locs[1], sizes[1], types[1], 2, m_ibo);
	m_mmdGroundShadowVAO = CreateVAO(buffers[2], locs[2], sizes[2], types[2], 1, m_ibo);
	for (const auto& mmdMat : m_mmdModel->m_materials) {
		GLFWMaterial mat(mmdMat);
		if (!mmdMat.m_texture.empty()) {
			auto [m_texture, m_hasAlpha] = m_viewer->GetTexture(mmdMat.m_texture);
			mat.m_texture = m_texture;
			mat.m_textureHasAlpha = m_hasAlpha;
		}
		if (!mmdMat.m_spTexture.empty())
			mat.m_spTexture = m_viewer->GetTexture(mmdMat.m_spTexture).m_texture;
		if (!mmdMat.m_toonTexture.empty())
			mat.m_toonTexture = m_viewer->GetTexture(mmdMat.m_toonTexture, true).m_texture;
		m_materials.emplace_back(mat);
	}
	return true;
}

void GLFWInstance::Clear() {
	if (m_posVBO != 0)
		glDeleteBuffers(1, &m_posVBO);
	if (m_norVBO != 0)
		glDeleteBuffers(1, &m_norVBO);
	if (m_uvVBO != 0)
		glDeleteBuffers(1, &m_uvVBO);
	if (m_ibo != 0)
		glDeleteBuffers(1, &m_ibo);
	m_posVBO = m_norVBO = m_uvVBO = m_ibo = 0;
	if (m_mmdVAO != 0)
		glDeleteVertexArrays(1, &m_mmdVAO);
	if (m_mmdEdgeVAO != 0)
		glDeleteVertexArrays(1, &m_mmdEdgeVAO);
	if (m_mmdGroundShadowVAO != 0)
		glDeleteVertexArrays(1, &m_mmdGroundShadowVAO);
	m_mmdVAO = m_mmdEdgeVAO = m_mmdGroundShadowVAO = 0;
}

void GLFWInstance::Update() const {
	m_mmdModel->Update();
	const size_t vtxCount = m_mmdModel->m_positions.size();
	glBindBuffer(GL_ARRAY_BUFFER, m_posVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount),
		m_mmdModel->m_updatePositions.data());
	glBindBuffer(GL_ARRAY_BUFFER, m_norVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount),
		m_mmdModel->m_updateNormals.data());
	glBindBuffer(GL_ARRAY_BUFFER, m_uvVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec2) * vtxCount),
		m_mmdModel->m_updateUVs.data());
}

void GLFWInstance::Draw() const {
	const auto& view = m_viewer->m_viewMat;
	const auto& proj = m_viewer->m_projMat;
	auto world = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));
	auto wv = view * world;
	auto wvp = proj * view * world;
	const auto& shader = m_viewer->m_shader;
	glm::vec3 lightColor = m_viewer->m_lightColor;
	glm::vec3 lightDir = glm::mat3(m_viewer->m_viewMat) * m_viewer->m_lightDir;
	glUseProgram(shader->m_prog);
	glUniformMatrix4fv(shader->m_uWV, 1, GL_FALSE, &wv[0][0]);
	glUniformMatrix4fv(shader->m_uWVP, 1, GL_FALSE, &wvp[0][0]);
	glUniform3fv(shader->m_uLightDir, 1, &lightDir[0]);
	glUniform3fv(shader->m_uLightColor, 1, &lightColor[0]);
	glBindVertexArray(m_mmdVAO);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		if (mmdMat.m_diffuse.a == 0)
			continue;
		glUniform3fv(shader->m_uAmbient, 1, &mmdMat.m_ambient[0]);
		glUniform3fv(shader->m_uDiffuse, 1, &mmdMat.m_diffuse[0]);
		glUniform3fv(shader->m_uSpecular, 1, &mmdMat.m_specular[0]);
		glUniform1f(shader->m_uSpecularPower, mmdMat.m_specularPower);
		glUniform1f(shader->m_uAlpha, mmdMat.m_diffuse.a);
		glActiveTexture(GL_TEXTURE0 + 0);
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
			glBindTexture(GL_TEXTURE_2D, m_viewer->m_dummyColorTex);
		}
		glActiveTexture(GL_TEXTURE0 + 1);
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
			glBindTexture(GL_TEXTURE_2D, m_viewer->m_dummyColorTex);
		}
		glActiveTexture(GL_TEXTURE0 + 2);
		if (mat.m_toonTexture != 0) {
			glUniform4fv(shader->m_uToonTexMulFactor, 1, &mmdMat.m_toonTextureMulFactor[0]);
			glUniform4fv(shader->m_uToonTexAddFactor, 1, &mmdMat.m_toonTextureAddFactor[0]);
			glUniform1i(shader->m_uToonTexMode, 1);
			glBindTexture(GL_TEXTURE_2D, mat.m_toonTexture);
		} else {
			glUniform1i(shader->m_uToonTexMode, 0);
			glBindTexture(GL_TEXTURE_2D, m_viewer->m_dummyColorTex);
		}
		if (mmdMat.m_bothFace)
			glDisable(GL_CULL_FACE);
		else {
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}
		size_t offset = m_beginIndex * m_mmdModel->m_indexElementSize;
		glDrawElements(GL_TRIANGLES, m_vertexCount, m_indexType, reinterpret_cast<GLvoid*>(offset));
	}
	const auto& edgeShader = m_viewer->m_edgeShader;
	glUseProgram(edgeShader->m_prog);
	glUniformMatrix4fv(edgeShader->m_uWV, 1, GL_FALSE, &wv[0][0]);
	glUniformMatrix4fv(edgeShader->m_uWVP, 1, GL_FALSE, &wvp[0][0]);
	glm::vec2 screenSize(m_viewer->m_screenWidth, m_viewer->m_screenHeight);
	glUniform2fv(edgeShader->m_uScreenSize, 1, &screenSize[0]);
	glBindVertexArray(m_mmdEdgeVAO);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		if (!mmdMat.m_edgeFlag)
			continue;
		if (mmdMat.m_diffuse.a == 0.0f)
			continue;
		glUniform1f(edgeShader->m_uEdgeSize, mmdMat.m_edgeSize);
		glUniform4fv(edgeShader->m_uEdgeColor, 1, &mmdMat.m_edgeColor[0]);
		size_t offset = m_beginIndex * m_mmdModel->m_indexElementSize;
		glDrawElements(GL_TRIANGLES, m_vertexCount, m_indexType, reinterpret_cast<GLvoid*>(offset));
	}
	const auto& gsShader = m_viewer->m_groundShadowShader;
	glUseProgram(gsShader->m_prog);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1, -1);
	glm::vec4 plane(0.f, 1.f, 0.f, 0.f);
	glm::vec4 light(-m_viewer->m_lightDir, 0.f);
	glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.0f) - glm::outerProduct(light, plane);
	glUniformMatrix4fv(gsShader->m_uWVP, 1, GL_FALSE, &(proj * view * shadow * world)[0][0]);
	glBindVertexArray(m_mmdGroundShadowVAO);
	auto shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
	glUniform4fv(gsShader->m_uShadowColor, 1, &shadowColor[0]);
	if (shadowColor.a < 1.0f) {
		glEnable(GL_BLEND);
		glEnable(GL_STENCIL_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glStencilFuncSeparate(GL_FRONT_AND_BACK, GL_NOTEQUAL, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	} else {
		glDisable(GL_BLEND);
		glDisable(GL_STENCIL_TEST);
	}
	glDisable(GL_CULL_FACE);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		if (!mmdMat.m_groundShadow)
			continue;
		if (mmdMat.m_diffuse.a == 0.0f)
			continue;
		size_t offset = m_beginIndex * m_mmdModel->m_indexElementSize;
		glDrawElements(GL_TRIANGLES, m_vertexCount, m_indexType, reinterpret_cast<GLvoid*>(offset));
	}
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
}

GLFWViewer::~GLFWViewer() {
	m_shader.reset();
	m_edgeShader.reset();
	m_groundShadowShader.reset();
	for (auto& [m_texture, m_hasAlpha] : m_textures | std::views::values)
		glDeleteTextures(1, &m_texture);
	m_textures.clear();
	if (m_dummyColorTex != 0)
		glDeleteTextures(1, &m_dummyColorTex);
	m_dummyColorTex = 0;
	m_vmdCameraAnim.reset();
}

void GLFWViewer::ConfigureGlfwHints() {
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, m_msaaSamples);
}

bool GLFWViewer::Setup() {
	glfwMakeContextCurrent(m_window);
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
		return false;
	glfwSwapInterval(0);
	glEnable(GL_MULTISAMPLE);
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
	return true;
}

bool GLFWViewer::Resize() {
	glViewport(0, 0, m_screenWidth, m_screenHeight);
	return true;
}

void GLFWViewer::BeginFrame() {
	glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

bool GLFWViewer::EndFrame() {
	glfwSwapBuffers(m_window);
	return true;
}

std::unique_ptr<Instance> GLFWViewer::CreateInstance() const {
	return std::make_unique<GLFWInstance>();
}

GLFWTexture GLFWViewer::GetTexture(const std::filesystem::path& texturePath, const bool clamp) {
	const auto it = m_textures.find(texturePath);
	if (it != m_textures.end())
		return it->second;
	int x = 0, y = 0, comp = 0;
	stbi_uc* image = LoadImageRGBA(texturePath, x, y, comp, true);
	if (!image)
		return GLFWTexture{ 0, false };
	const bool hasAlpha = comp == 4;
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	stbi_image_free(image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (clamp) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	m_textures[texturePath] = GLFWTexture{ tex, hasAlpha };
	return m_textures[texturePath];
}
