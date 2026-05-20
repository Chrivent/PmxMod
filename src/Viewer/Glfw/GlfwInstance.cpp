#include "GlfwInstance.h"

#include "GlfwDrawer.h"

#include "GlfwViewer.h"
#include "../../Model/ModelPose.h"

namespace Chrivent {
	GlfwDrawer::GlfwDrawer(const GlfwInstance& sourceInstance) : instance(sourceInstance) {}

	void GlfwDrawer::DrawModel() const {
		const auto& info = instance.GetGlfwInfo();
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto vao = info.vao;
		const auto indexType = info.indexType;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
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
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
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
			const size_t offset = beginIndex * info.model->geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void GlfwDrawer::DrawEdge() const {
		const auto& info = instance.GetGlfwInfo();
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto edgeVao = info.edgeVao;
		const auto indexType = info.indexType;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
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
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag)
				continue;
			if (mat.diffuse.a == 0.0f)
				continue;
			glUniform1f(edgeShader->edgeSizeLocation, mat.edgeSize);
			glUniform4fv(edgeShader->edgeColorLocation, 1, &mat.edgeColor[0]);
			const size_t offset = beginIndex * info.model->geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void GlfwDrawer::DrawGroundShadow() const {
		const auto& info = instance.GetGlfwInfo();
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto gsVao = info.gsVao;
		const auto indexType = info.indexType;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
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
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow)
				continue;
			if (mat.diffuse.a == 0.0f)
				continue;
			const size_t offset = beginIndex * info.model->geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
		glDisable(GL_POLYGON_OFFSET_FILL);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
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

	GlfwInstance::GlfwInstance() {
		info = std::make_unique<GlfwInstanceInfo>();
	}

	GlfwInstance::~GlfwInstance() {
		GlfwInstance::Clear();
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
		auto& info = GetGlfwInfo();
		if (info.vao != 0)
			glDeleteVertexArrays(1, &info.vao);
		if (info.edgeVao != 0)
			glDeleteVertexArrays(1, &info.edgeVao);
		if (info.gsVao != 0)
			glDeleteVertexArrays(1, &info.gsVao);
		info.vao = info.edgeVao = info.gsVao = 0;
	}

	bool GlfwInstance::Setup(Viewer& baseViewer) {
		auto& info = GetGlfwInfo();
		info.viewer = &dynamic_cast<GlfwViewer&>(baseViewer);
		if (info.model == nullptr)
			return false;
		drawer = std::make_unique<GlfwDrawer>(*this);
		const size_t vtxCount = info.model->geometryData.positions.size();
		posVbo = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
		norVbo = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
		uvVbo  = CreateBuffer(GL_ARRAY_BUFFER, sizeof(glm::vec2) * vtxCount, nullptr, GL_DYNAMIC_DRAW);
		const size_t idxSize = info.model->geometryData.indexElementSize;
		const size_t idxCount = info.model->geometryData.indexCount;
		ibo = CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, idxSize * idxCount, info.model->geometryData.indices.data(), GL_STATIC_DRAW);
		if (idxSize == 1)
			info.indexType = GL_UNSIGNED_BYTE;
		else if (idxSize == 2)
			info.indexType = GL_UNSIGNED_SHORT;
		else if (idxSize == 4)
			info.indexType = GL_UNSIGNED_INT;
		else
			return false;
		const GLuint buffers[][3]   = {
			{ posVbo, norVbo, uvVbo },
			{ posVbo, norVbo },
			{ posVbo }
		};
		const GLint locs[][3] = {
			{ info.viewer->shader->positionLocation, info.viewer->shader->normalLocation, info.viewer->shader->uvLocation },
			{ info.viewer->edgeShader->positionLocation, info.viewer->edgeShader->normalLocation },
			{ info.viewer->gsShader->positionLocation }
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
		info.vao = CreateVao(buffers[0], locs[0], sizes[0], types[0], 3, ibo);
		info.edgeVao = CreateVao(buffers[1], locs[1], sizes[1], types[1], 2, ibo);
		info.gsVao = CreateVao(buffers[2], locs[2], sizes[2], types[2], 1, ibo);
		for (const auto& mat : info.model->materialData.materials) {
			GlfwViewerMaterial material(mat);
			if (!mat.texture.empty()) {
				const auto texture = info.viewer->LoadTexture(mat.texture);
				material.texture = texture.texture;
				material.textureHasAlpha = texture.hasAlpha;
			}
			if (!mat.spTexture.empty())
				material.sphereTexture = info.viewer->LoadTexture(mat.spTexture).texture;
			if (!mat.cartoonTexture.empty())
				material.cartoonTexture = info.viewer->LoadTexture(mat.cartoonTexture, true).texture;
			info.materials.emplace_back(material);
		}
		return true;
	}

	void GlfwInstance::Update() const {
		const auto& info = GetGlfwInfo();
		const ModelPose pose(*info.model);
		pose.Update();
		const size_t vtxCount = info.model->geometryData.positions.size();
		glBindBuffer(GL_ARRAY_BUFFER, posVbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount),
			info.model->geometryData.updatePositions.data());
		glBindBuffer(GL_ARRAY_BUFFER, norVbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec3) * vtxCount),
			info.model->geometryData.updateNormals.data());
		glBindBuffer(GL_ARRAY_BUFFER, uvVbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec2) * vtxCount),
			info.model->geometryData.updateUVs.data());
	}
}
