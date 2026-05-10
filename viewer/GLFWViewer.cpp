#include "GLFWViewer.h"

#include "../src/Model.h"

#include <fstream>
#include <ranges>
#include <stb_image.h>

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

void LoadUniformLocations(const GLuint prog, const char* const* names, GLint* const* outs, const int count) {
	for (int i = 0; i < count; i++)
		*outs[i] = glGetUniformLocation(prog, names[i]);
}

void* LoadGlProc(const char* name) {
	return reinterpret_cast<void*>(glfwGetProcAddress(name));
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
	if (program != 0)
		glDeleteProgram(program);
	program = 0;
}

bool GLFWShader::Setup(const GLFWViewer& viewer) {
	program = CreateShader(viewer.m_shaderDir / "mmd.glsl");
	if (program == 0)
		return false;
	positionLocation = glGetAttribLocation(program, "position");
	normalLocation = glGetAttribLocation(program, "normal");
	uvLocation  = glGetAttribLocation(program, "uv");
	const char* names[] = {
		"wv", "wvp",
		"ambient", "diffuse", "specular", "specularPower", "alpha",
		"texMode", "tex", "texMulFactor", "texAddFactor",
		"sphereTexMode", "sphereTex", "sphereTexMulFactor", "sphereTexAddFactor",
		"toonTexMode", "toonTex", "toonTexMulFactor", "toonTexAddFactor",
		"lightColor", "lightDir"
	};
	GLint* outs[] = {
		&wvLocation, &wvpLocation,
		&ambientLocation, &diffuseLocation, &specularLocation, &specularPowerLocation, &alphaLocation,
		&texModeLocation, &texLocation, &texMulFactorLocation, &texAddFactorLocation,
		&sphereTexModeLocation, &sphereTexLocation, &sphereTexMulFactorLocation, &sphereTexAddFactorLocation,
		&toonTexModeLocation, &toonTexLocation, &toonTexMulFactorLocation, &toonTexAddFactorLocation,
		&lightColorLocation, &lightDirLocation
	};
	LoadUniformLocations(program, names, outs, std::size(names));
	glUseProgram(program);
	glUniform1i(texLocation, 0);
	glUniform1i(sphereTexLocation, 1);
	glUniform1i(toonTexLocation, 2);
	return true;
}

GLFWEdgeShader::~GLFWEdgeShader() {
	if (program != 0)
		glDeleteProgram(program);
	program = 0;
}

bool GLFWEdgeShader::Setup(const GLFWViewer& viewer) {
	program = CreateShader(viewer.m_shaderDir / "mmd_edge.glsl");
	if (program == 0)
		return false;
	positionLocation = glGetAttribLocation(program, "position");
	normalLocation = glGetAttribLocation(program, "normal");
	wvLocation = glGetUniformLocation(program, "wv");
	wvpLocation = glGetUniformLocation(program, "wvp");
	screenSizeLocation = glGetUniformLocation(program, "screenSize");
	edgeSizeLocation = glGetUniformLocation(program, "edgeSize");
	edgeColorLocation = glGetUniformLocation(program, "edgeColor");
	return true;
}

GLFWGroundShadowShader::~GLFWGroundShadowShader() {
	if (program != 0)
		glDeleteProgram(program);
	program = 0;
}

bool GLFWGroundShadowShader::Setup(const GLFWViewer& viewer) {
	program = CreateShader(viewer.m_shaderDir / "mmd_ground_shadow.glsl");
	if (program == 0)
		return false;
	positionLocation = glGetAttribLocation(program, "position");
	wvpLocation = glGetUniformLocation(program, "wvp");
	shadowColorLocation = glGetUniformLocation(program, "shadowColor");
	return true;
}

GLFWMaterial::GLFWMaterial(const Material &mat)
	: mat(mat) {
}

bool GLFWInstance::Setup(Viewer& viewer) {
	m_viewer = &dynamic_cast<GLFWViewer&>(viewer);
	if (m_model == nullptr)
		return false;
	const size_t vtxCount = m_model->positions.size();
	m_posVbo = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
	m_norVbo = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
	m_uvVbo  = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec2) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
	const size_t idxSize = m_model->indexElementSize;
	const size_t idxCount = m_model->indexCount;
	m_ibo = CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, idxSize * idxCount, m_model->indices.data(), GL_STATIC_DRAW);
	if (idxSize == 1)
		m_indexType = GL_UNSIGNED_BYTE;
	else if (idxSize == 2)
		m_indexType = GL_UNSIGNED_SHORT;
	else if (idxSize == 4)
		m_indexType = GL_UNSIGNED_INT;
	else
		return false;
	const GLuint buffers[][3]   = {
		{ m_posVbo, m_norVbo, m_uvVbo },
		{ m_posVbo, m_norVbo },
		{ m_posVbo }
	};
	const GLint locs[][3] = {
		{ m_viewer->m_shader->positionLocation, m_viewer->m_shader->normalLocation, m_viewer->m_shader->uvLocation },
		{ m_viewer->m_edgeShader->positionLocation, m_viewer->m_edgeShader->normalLocation },
		{ m_viewer->m_gsShader->positionLocation }
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
	m_vao = CreateVAO(buffers[0], locs[0], sizes[0], types[0], 3, m_ibo);
	m_edgeVao = CreateVAO(buffers[1], locs[1], sizes[1], types[1], 2, m_ibo);
	m_gsVao = CreateVAO(buffers[2], locs[2], sizes[2], types[2], 1, m_ibo);
	for (const auto& mat : m_model->materials) {
		GLFWMaterial m(mat);
		if (!mat.texture.empty()) {
			auto [texture, hasAlpha] = m_viewer->LoadTexture(mat.texture);
			m.texture = texture;
			m.textureHasAlpha = hasAlpha;
		}
		if (!mat.spTexture.empty())
			m.spTexture = m_viewer->LoadTexture(mat.spTexture).texture;
		if (!mat.cartoonTexture.empty())
			m.toonTexture = m_viewer->LoadTexture(mat.cartoonTexture, true).texture;
		m_materials.emplace_back(m);
	}
	return true;
}

void GLFWInstance::Clear() {
	if (m_posVbo != 0)
		glDeleteBuffers(1, &m_posVbo);
	if (m_norVbo != 0)
		glDeleteBuffers(1, &m_norVbo);
	if (m_uvVbo != 0)
		glDeleteBuffers(1, &m_uvVbo);
	if (m_ibo != 0)
		glDeleteBuffers(1, &m_ibo);
	m_posVbo = m_norVbo = m_uvVbo = m_ibo = 0;
	if (m_vao != 0)
		glDeleteVertexArrays(1, &m_vao);
	if (m_edgeVao != 0)
		glDeleteVertexArrays(1, &m_edgeVao);
	if (m_gsVao != 0)
		glDeleteVertexArrays(1, &m_gsVao);
	m_vao = m_edgeVao = m_gsVao = 0;
}

void GLFWInstance::Update() const {
	m_model->Update();
	const size_t vtxCount = m_model->positions.size();
	glBindBuffer(GL_ARRAY_BUFFER, m_posVbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount),
		m_model->updatePositions.data());
	glBindBuffer(GL_ARRAY_BUFFER, m_norVbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount),
		m_model->updateNormals.data());
	glBindBuffer(GL_ARRAY_BUFFER, m_uvVbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec2) * vtxCount),
		m_model->updateUVs.data());
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
	glUseProgram(shader->program);
	glUniformMatrix4fv(shader->wvLocation, 1, GL_FALSE, &wv[0][0]);
	glUniformMatrix4fv(shader->wvpLocation, 1, GL_FALSE, &wvp[0][0]);
	glUniform3fv(shader->lightDirLocation, 1, &lightDir[0]);
	glUniform3fv(shader->lightColorLocation, 1, &lightColor[0]);
	glBindVertexArray(m_vao);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for (const auto& [beginIndex, indexCount, materialId] : m_model->subMeshes) {
		const auto& m = m_materials[materialId];
		const auto& mat = m.mat;
		if (mat.diffuse.a == 0)
			continue;
		glUniform3fv(shader->ambientLocation, 1, &mat.ambient[0]);
		glUniform3fv(shader->diffuseLocation, 1, &mat.diffuse[0]);
		glUniform3fv(shader->specularLocation, 1, &mat.specular[0]);
		glUniform1f(shader->specularPowerLocation, mat.specularPower);
		glUniform1f(shader->alphaLocation, mat.diffuse.a);
		glActiveTexture(GL_TEXTURE0 + 0);
		if (m.texture != 0) {
			if (!m.textureHasAlpha)
				glUniform1i(shader->texModeLocation, 1);
			else
				glUniform1i(shader->texModeLocation, 2);
			glUniform4fv(shader->texMulFactorLocation, 1, &mat.textureMulFactor[0]);
			glUniform4fv(shader->texAddFactorLocation, 1, &mat.textureAddFactor[0]);
			glBindTexture(GL_TEXTURE_2D, m.texture);
		} else {
			glUniform1i(shader->texModeLocation, 0);
			glBindTexture(GL_TEXTURE_2D, m_viewer->m_dummyColorTex);
		}
		glActiveTexture(GL_TEXTURE0 + 1);
		if (m.spTexture != 0) {
			if (mat.spTextureMode == SphereMode::Mul)
				glUniform1i(shader->sphereTexModeLocation, 1);
			else if (mat.spTextureMode == SphereMode::Add)
				glUniform1i(shader->sphereTexModeLocation, 2);
			glUniform4fv(shader->sphereTexMulFactorLocation, 1, &mat.sphereTextureMulFactor[0]);
			glUniform4fv(shader->sphereTexAddFactorLocation, 1, &mat.sphereTextureAddFactor[0]);
			glBindTexture(GL_TEXTURE_2D, m.spTexture);
		} else {
			glUniform1i(shader->sphereTexModeLocation, 0);
			glBindTexture(GL_TEXTURE_2D, m_viewer->m_dummyColorTex);
		}
		glActiveTexture(GL_TEXTURE0 + 2);
		if (m.toonTexture != 0) {
			glUniform4fv(shader->toonTexMulFactorLocation, 1, &mat.cartoonTextureMulFactor[0]);
			glUniform4fv(shader->toonTexAddFactorLocation, 1, &mat.cartoonTextureAddFactor[0]);
			glUniform1i(shader->toonTexModeLocation, 1);
			glBindTexture(GL_TEXTURE_2D, m.toonTexture);
		} else {
			glUniform1i(shader->toonTexModeLocation, 0);
			glBindTexture(GL_TEXTURE_2D, m_viewer->m_dummyColorTex);
		}
		if (mat.bothFace)
			glDisable(GL_CULL_FACE);
		else {
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}
		size_t offset = beginIndex * m_model->indexElementSize;
		glDrawElements(GL_TRIANGLES, indexCount, m_indexType, reinterpret_cast<GLvoid*>(offset));
	}
	const auto& edgeShader = m_viewer->m_edgeShader;
	glUseProgram(edgeShader->program);
	glUniformMatrix4fv(edgeShader->wvLocation, 1, GL_FALSE, &wv[0][0]);
	glUniformMatrix4fv(edgeShader->wvpLocation, 1, GL_FALSE, &wvp[0][0]);
	glm::vec2 screenSize(m_viewer->m_screenWidth, m_viewer->m_screenHeight);
	glUniform2fv(edgeShader->screenSizeLocation, 1, &screenSize[0]);
	glBindVertexArray(m_edgeVao);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	for (const auto& [beginIndex, indexCount, materialId] : m_model->subMeshes) {
		const auto& m = m_materials[materialId];
		const auto& mat = m.mat;
		if (!mat.edgeFlag)
			continue;
		if (mat.diffuse.a == 0.0f)
			continue;
		glUniform1f(edgeShader->edgeSizeLocation, mat.edgeSize);
		glUniform4fv(edgeShader->edgeColorLocation, 1, &mat.edgeColor[0]);
		size_t offset = beginIndex * m_model->indexElementSize;
		glDrawElements(GL_TRIANGLES, indexCount, m_indexType, reinterpret_cast<GLvoid*>(offset));
	}
	const auto& gsShader = m_viewer->m_gsShader;
	glUseProgram(gsShader->program);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1, -1);
	glm::vec4 plane(0.f, 1.f, 0.f, 0.f);
	glm::vec4 light(-m_viewer->m_lightDir, 0.f);
	glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.0f) - glm::outerProduct(light, plane);
	glUniformMatrix4fv(gsShader->wvpLocation, 1, GL_FALSE, &(proj * view * shadow * world)[0][0]);
	glBindVertexArray(m_gsVao);
	auto shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
	glUniform4fv(gsShader->shadowColorLocation, 1, &shadowColor[0]);
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
	for (const auto& [beginIndex, indexCount, materialId] : m_model->subMeshes) {
		const auto& m = m_materials[materialId];
		const auto& mat = m.mat;
		if (!mat.groundShadow)
			continue;
		if (mat.diffuse.a == 0.0f)
			continue;
		size_t offset = beginIndex * m_model->indexElementSize;
		glDrawElements(GL_TRIANGLES, indexCount, m_indexType, reinterpret_cast<GLvoid*>(offset));
	}
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
}

GLFWViewer::~GLFWViewer() {
	for (auto& textureInfo : m_textures | std::views::values)
		glDeleteTextures(1, &textureInfo.texture);
	if (m_dummyColorTex != 0)
		glDeleteTextures(1, &m_dummyColorTex);
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
	if (!gladLoadGLLoader(LoadGlProc))
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
	m_gsShader = std::make_unique<GLFWGroundShadowShader>();
	if (!m_gsShader->Setup(*this))
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

GLFWTexture GLFWViewer::LoadTexture(const std::filesystem::path& texturePath, const bool clamp) {
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

