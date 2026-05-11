#include "GlfwViewer.h"

#include "../src/Model.h"

#include <fstream>
#include <ranges>
#include <stb_image.h>

GLuint GlfwShaderHelper::CompileShader(const GLenum shaderType, const std::string& code) {
	const GLuint shader = glCreateShader(shaderType);
	if (!shader)
		return 0;
	const char* codes = code.c_str();
	const auto codesLen = static_cast<GLint>(code.size());
	glShaderSource(shader, 1, &codes, &codesLen);
	glCompileShader(shader);
	return shader;
}

GLuint GlfwInstance::CreateBuffer(const GLenum target, const size_t size, const void* data, const GLenum usage) {
	GLuint b = 0;
	glGenBuffers(1, &b);
	glBindBuffer(target, b);
	glBufferData(target, static_cast<GLsizeiptr>(size), data, usage);
	return b;
}

GLuint GlfwInstance::CreateVao(const GLuint* buffers, const GLint* locs, const GLint* sizes, const GLenum* types,
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

void GlfwShader::LoadUniformLocations(const GLuint prog, const char* const* names, GLint* const* outs, const int count) {
	for (int i = 0; i < count; i++)
		*outs[i] = glGetUniformLocation(prog, names[i]);
}

void* GlfwViewer::LoadGlProc(const char* name) {
	return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

std::string GlfwShaderHelper::InjectDefine(const std::string& src, const char* defineLine) {
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

GLuint GlfwShaderHelper::CreateShader(const std::filesystem::path& file) {
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

GlfwShader::~GlfwShader() {
	if (program != 0)
		glDeleteProgram(program);
	program = 0;
}

bool GlfwShader::Setup(const GlfwViewer& viewer) {
	program = GlfwShaderHelper::CreateShader(viewer.shaderDir / "mmd.glsl");
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
		"cartoonTexMode", "cartoonTex", "cartoonTexMulFactor", "cartoonTexAddFactor",
		"lightColor", "lightDir"
	};
	GLint* outs[] = {
		&wvLocation, &wvpLocation,
		&ambientLocation, &diffuseLocation, &specularLocation, &specularPowerLocation, &alphaLocation,
		&texModeLocation, &texLocation, &texMulFactorLocation, &texAddFactorLocation,
		&sphereTexModeLocation, &sphereTexLocation, &sphereTexMulFactorLocation, &sphereTexAddFactorLocation,
		&cartoonTexModeLocation, &cartoonTexLocation, &cartoonTexMulFactorLocation, &cartoonTexAddFactorLocation,
		&lightColorLocation, &lightDirLocation
	};
	LoadUniformLocations(program, names, outs, std::size(names));
	glUseProgram(program);
	glUniform1i(texLocation, 0);
	glUniform1i(sphereTexLocation, 1);
	glUniform1i(cartoonTexLocation, 2);
	return true;
}

GlfwEdgeShader::~GlfwEdgeShader() {
	if (program != 0)
		glDeleteProgram(program);
	program = 0;
}

bool GlfwEdgeShader::Setup(const GlfwViewer& viewer) {
	program = GlfwShaderHelper::CreateShader(viewer.shaderDir / "mmd_edge.glsl");
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

GlfwGroundShadowShader::~GlfwGroundShadowShader() {
	if (program != 0)
		glDeleteProgram(program);
	program = 0;
}

bool GlfwGroundShadowShader::Setup(const GlfwViewer& viewer) {
	program = GlfwShaderHelper::CreateShader(viewer.shaderDir / "mmd_ground_shadow.glsl");
	if (program == 0)
		return false;
	positionLocation = glGetAttribLocation(program, "position");
	wvpLocation = glGetUniformLocation(program, "wvp");
	shadowColorLocation = glGetUniformLocation(program, "shadowColor");
	return true;
}

GlfwMaterial::GlfwMaterial(const Material& sourceMat)
	: mat(sourceMat) {
}

GlfwInstance::~GlfwInstance() {
	GlfwInstance::Clear();
}

bool GlfwInstance::Setup(Viewer& baseViewer) {
	viewer = &dynamic_cast<GlfwViewer&>(baseViewer);
	if (model == nullptr)
		return false;
	const size_t vtxCount = model->positions.size();
	posVbo = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
	norVbo = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
	uvVbo  = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec2) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
	const size_t idxSize = model->indexElementSize;
	const size_t idxCount = model->indexCount;
	ibo = CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, idxSize * idxCount, model->indices.data(), GL_STATIC_DRAW);
	if (idxSize == 1)
		indexType = GL_UNSIGNED_BYTE;
	else if (idxSize == 2)
		indexType = GL_UNSIGNED_SHORT;
	else if (idxSize == 4)
		indexType = GL_UNSIGNED_INT;
	else
		return false;
	const GLuint buffers[][3]   = {
		{ posVbo, norVbo, uvVbo },
		{ posVbo, norVbo },
		{ posVbo }
	};
	const GLint locs[][3] = {
		{ viewer->shader->positionLocation, viewer->shader->normalLocation, viewer->shader->uvLocation },
		{ viewer->edgeShader->positionLocation, viewer->edgeShader->normalLocation },
		{ viewer->gsShader->positionLocation }
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
	vao = CreateVao(buffers[0], locs[0], sizes[0], types[0], 3, ibo);
	edgeVao = CreateVao(buffers[1], locs[1], sizes[1], types[1], 2, ibo);
	gsVao = CreateVao(buffers[2], locs[2], sizes[2], types[2], 1, ibo);
	for (const auto& mat : model->materials) {
		GlfwMaterial material(mat);
		if (!mat.texture.empty()) {
			auto [texture, hasAlpha] = viewer->LoadTexture(mat.texture);
			material.texture = texture;
			material.textureHasAlpha = hasAlpha;
		}
		if (!mat.spTexture.empty())
			material.sphereTexture = viewer->LoadTexture(mat.spTexture).texture;
		if (!mat.cartoonTexture.empty())
			material.cartoonTexture = viewer->LoadTexture(mat.cartoonTexture, true).texture;
		materials.emplace_back(material);
	}
	return true;
}

void GlfwInstance::Clear() {
	if (posVbo != 0)
		glDeleteBuffers(1, &posVbo);
	if (norVbo != 0)
		glDeleteBuffers(1, &norVbo);
	if (uvVbo != 0)
		glDeleteBuffers(1, &uvVbo);
	if (ibo != 0)
		glDeleteBuffers(1, &ibo);
	posVbo = norVbo = uvVbo = ibo = 0;
	if (vao != 0)
		glDeleteVertexArrays(1, &vao);
	if (edgeVao != 0)
		glDeleteVertexArrays(1, &edgeVao);
	if (gsVao != 0)
		glDeleteVertexArrays(1, &gsVao);
	vao = edgeVao = gsVao = 0;
}

void GlfwInstance::Update() const {
	model->Update();
	const size_t vtxCount = model->positions.size();
	glBindBuffer(GL_ARRAY_BUFFER, posVbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount),
		model->updatePositions.data());
	glBindBuffer(GL_ARRAY_BUFFER, norVbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount),
		model->updateNormals.data());
	glBindBuffer(GL_ARRAY_BUFFER, uvVbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec2) * vtxCount),
		model->updateUVs.data());
}

void GlfwInstance::DrawModel() const {
	const auto& view = viewer->viewMat;
	const auto& proj = viewer->projMat;
	const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
	auto wv = view * world;
	auto wvp = proj * view * world;
	const auto& shader = viewer->shader;
	glm::vec3 lightColor = viewer->lightColor;
	glm::vec3 lightDir = glm::mat3(viewer->viewMat) * viewer->lightDir;
	glUseProgram(shader->program);
	glUniformMatrix4fv(shader->wvLocation, 1, GL_FALSE, &wv[0][0]);
	glUniformMatrix4fv(shader->wvpLocation, 1, GL_FALSE, &wvp[0][0]);
	glUniform3fv(shader->lightDirLocation, 1, &lightDir[0]);
	glUniform3fv(shader->lightColorLocation, 1, &lightColor[0]);
	glBindVertexArray(vao);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for (const auto& [beginIndex, indexCount, materialId] : model->subMeshes) {
		const auto& material = materials[materialId];
		const auto& mat = material.mat;
		if (mat.diffuse.a == 0)
			continue;
		glUniform3fv(shader->ambientLocation, 1, &mat.ambient[0]);
		glUniform3fv(shader->diffuseLocation, 1, &mat.diffuse[0]);
		glUniform3fv(shader->specularLocation, 1, &mat.specular[0]);
		glUniform1f(shader->specularPowerLocation, mat.specularPower);
		glUniform1f(shader->alphaLocation, mat.diffuse.a);
		glActiveTexture(GL_TEXTURE0 + 0);
		if (material.texture != 0) {
			if (!material.textureHasAlpha)
				glUniform1i(shader->texModeLocation, 1);
			else
				glUniform1i(shader->texModeLocation, 2);
			glUniform4fv(shader->texMulFactorLocation, 1, &mat.textureMulFactor[0]);
			glUniform4fv(shader->texAddFactorLocation, 1, &mat.textureAddFactor[0]);
			glBindTexture(GL_TEXTURE_2D, material.texture);
		} else {
			glUniform1i(shader->texModeLocation, 0);
			glBindTexture(GL_TEXTURE_2D, viewer->dummyColorTex);
		}
		glActiveTexture(GL_TEXTURE0 + 1);
		if (material.sphereTexture != 0) {
			if (mat.spTextureMode == SphereMode::Mul)
				glUniform1i(shader->sphereTexModeLocation, 1);
			else if (mat.spTextureMode == SphereMode::Add)
				glUniform1i(shader->sphereTexModeLocation, 2);
			glUniform4fv(shader->sphereTexMulFactorLocation, 1, &mat.sphereTextureMulFactor[0]);
			glUniform4fv(shader->sphereTexAddFactorLocation, 1, &mat.sphereTextureAddFactor[0]);
			glBindTexture(GL_TEXTURE_2D, material.sphereTexture);
		} else {
			glUniform1i(shader->sphereTexModeLocation, 0);
			glBindTexture(GL_TEXTURE_2D, viewer->dummyColorTex);
		}
		glActiveTexture(GL_TEXTURE0 + 2);
		if (material.cartoonTexture != 0) {
			glUniform4fv(shader->cartoonTexMulFactorLocation, 1, &mat.cartoonTextureMulFactor[0]);
			glUniform4fv(shader->cartoonTexAddFactorLocation, 1, &mat.cartoonTextureAddFactor[0]);
			glUniform1i(shader->cartoonTexModeLocation, 1);
			glBindTexture(GL_TEXTURE_2D, material.cartoonTexture);
		} else {
			glUniform1i(shader->cartoonTexModeLocation, 0);
			glBindTexture(GL_TEXTURE_2D, viewer->dummyColorTex);
		}
		if (mat.bothFace)
			glDisable(GL_CULL_FACE);
		else {
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}
		const size_t offset = beginIndex * model->indexElementSize;
		glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
	}
}

void GlfwInstance::DrawEdge() const {
	const auto& view = viewer->viewMat;
	const auto& proj = viewer->projMat;
	const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
	auto wv = view * world;
	auto wvp = proj * view * world;
	const auto& edgeShader = viewer->edgeShader;
	glUseProgram(edgeShader->program);
	glUniformMatrix4fv(edgeShader->wvLocation, 1, GL_FALSE, &wv[0][0]);
	glUniformMatrix4fv(edgeShader->wvpLocation, 1, GL_FALSE, &wvp[0][0]);
	glm::vec2 screenSize(viewer->screenWidth, viewer->screenHeight);
	glUniform2fv(edgeShader->screenSizeLocation, 1, &screenSize[0]);
	glBindVertexArray(edgeVao);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	for (const auto& [beginIndex, indexCount, materialId] : model->subMeshes) {
		const auto& material = materials[materialId];
		const auto& mat = material.mat;
		if (!mat.edgeFlag)
			continue;
		if (mat.diffuse.a == 0.0f)
			continue;
		glUniform1f(edgeShader->edgeSizeLocation, mat.edgeSize);
		glUniform4fv(edgeShader->edgeColorLocation, 1, &mat.edgeColor[0]);
		const size_t offset = beginIndex * model->indexElementSize;
		glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
	}
}

void GlfwInstance::DrawGroundShadow() const {
	const auto& view = viewer->viewMat;
	const auto& proj = viewer->projMat;
	const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
	const auto& gsShader = viewer->gsShader;
	glUseProgram(gsShader->program);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1, -1);
	constexpr glm::vec4 plane(0.f, 1.f, 0.f, 0.f);
	const glm::vec4 light(-viewer->lightDir, 0.f);
	const glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.0f) - glm::outerProduct(light, plane);
	glUniformMatrix4fv(gsShader->wvpLocation, 1, GL_FALSE, &(proj * view * shadow * world)[0][0]);
	glBindVertexArray(gsVao);
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
	for (const auto& [beginIndex, indexCount, materialId] : model->subMeshes) {
		const auto& material = materials[materialId];
		const auto& mat = material.mat;
		if (!mat.groundShadow)
			continue;
		if (mat.diffuse.a == 0.0f)
			continue;
		const size_t offset = beginIndex * model->indexElementSize;
		glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
	}
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
}

GlfwViewer::~GlfwViewer() {
	for (auto& [texture, hasAlpha] : textures | std::views::values)
		glDeleteTextures(1, &texture);
	if (dummyColorTex != 0)
		glDeleteTextures(1, &dummyColorTex);
}

void GlfwViewer::ConfigureGlfwHints() {
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, msaaSamples);
}

bool GlfwViewer::Setup() {
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader(LoadGlProc))
		return false;
	glfwSwapInterval(0);
	glEnable(GL_MULTISAMPLE);
	InitDirs("shader_Glfw");
	shader = std::make_unique<GlfwShader>();
	if (!shader->Setup(*this))
		return false;
	edgeShader = std::make_unique<GlfwEdgeShader>();
	if (!edgeShader->Setup(*this))
		return false;
	gsShader = std::make_unique<GlfwGroundShadowShader>();
	if (!gsShader->Setup(*this))
		return false;
	glGenTextures(1, &dummyColorTex);
	glBindTexture(GL_TEXTURE_2D, dummyColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);
	return true;
}

bool GlfwViewer::Resize() {
	glViewport(0, 0, screenWidth, screenHeight);
	return true;
}

void GlfwViewer::BeginFrame() {
	glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

bool GlfwViewer::EndFrame() {
	glfwSwapBuffers(window);
	return true;
}

std::unique_ptr<Instance> GlfwViewer::CreateInstance() const {
	return std::make_unique<GlfwInstance>();
}

GlfwTexture GlfwViewer::LoadTexture(const std::filesystem::path& texturePath, const bool clamp) {
	const auto it = textures.find(texturePath);
	if (it != textures.end())
		return it->second;
	int x = 0, y = 0, comp = 0;
	stbi_uc* image = LoadImageRgba(texturePath, x, y, comp, true);
	if (!image)
		return GlfwTexture{ 0, false };
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
	textures[texturePath] = GlfwTexture{ tex, hasAlpha };
	return textures[texturePath];
}

