#include "Viewer/Glfw/GlfwDrawer.h"

#include "Viewer/Glfw/GlfwInstance.h"
#include "Viewer/Glfw/GlfwViewer.h"
#include "Core/Model/Model.h"
#include "Viewer/Shader/ShaderConstants.h"

namespace Chrivent {
	void GlfwDrawer::BeginDynamicBufferFrame() const {
		instance.vertexConstantsRing.BeginFrame(0);
		instance.pixelConstantsRing.BeginFrame(0);
	}

	bool GlfwDrawer::UpdateUniformBuffer(GlfwDynamicBufferRing& ring, const GLuint binding, const void* data, const size_t size) const {
		std::string error;
		const auto slice = ring.Allocate(size, instance.uniformBufferOffsetAlignment, error);
		if (!slice.has_value())
			return false;
		glNamedBufferSubData(ring.GetBuffer(), slice->offset, slice->size, data);
		glBindBufferRange(GL_UNIFORM_BUFFER, binding, ring.GetBuffer(), slice->offset, slice->size);
		return true;
	}

	void GlfwDrawer::DrawModel() {
		BeginDynamicBufferFrame();
		const auto* viewer = instance.viewer;
		if (!viewer->modelEffectEnabled)
			return;
		const auto& materials = instance.materials;
		const auto indexType = instance.indexType;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		ModelVertexConstants vertexConstants;
		vertexConstants.wv = view * world;
		vertexConstants.wvp = proj * view * world;
		const auto& shader = viewer->shader;
		const glm::vec3 lightColor = viewer->lightColor;
		const glm::vec3 lightDir = glm::mat3(viewer->viewMat) * viewer->lightDir;
		ModelPixelConstants basePixelConstants{};
		basePixelConstants.lightColor = glm::vec4(lightColor, 0.0f);
		basePixelConstants.lightDir = glm::vec4(lightDir, 0.0f);
		glUseProgram(shader->program);
		if (!UpdateUniformBuffer(instance.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)))
			return;
		glBindVertexArray(instance.vao);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_POLYGON_OFFSET_FILL);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
		glCullFace(GL_BACK);
		bool cullEnabled = true;
		GLenum cullFaceMode = GL_BACK;
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			ModelPixelConstants pixelConstants = basePixelConstants;
			pixelConstants.diffuseAlpha = mat.diffuse;
			pixelConstants.ambientSpecularPower = glm::vec4(mat.ambient, mat.specularPower);
			pixelConstants.specular = glm::vec4(mat.specular, 0.0f);
			pixelConstants.texMulFactor = mat.textureMulFactor;
			pixelConstants.texAddFactor = mat.textureAddFactor;
			pixelConstants.toonTexMulFactor = mat.toonTextureMulFactor;
			pixelConstants.toonTexAddFactor = mat.toonTextureAddFactor;
			pixelConstants.sphereTexMulFactor = mat.sphereTextureMulFactor;
			pixelConstants.sphereTexAddFactor = mat.sphereTextureAddFactor;
			GLuint baseTexture = viewer->dummyColorTex;
			if (material.texture != 0) {
				if (!material.textureHasAlpha)
					pixelConstants.textureModes.x = 1;
				else
					pixelConstants.textureModes.x = 2;
				baseTexture = material.texture;
			}
			glBindTextureUnit(0, baseTexture);
			GLuint toonTexture = viewer->dummyColorTex;
			if (material.toonTexture != 0) {
				pixelConstants.textureModes.y = 1;
				toonTexture = material.toonTexture;
			}
			glBindTextureUnit(1, toonTexture);
			GLuint sphereTexture = viewer->dummyColorTex;
			if (material.sphereTexture != 0) {
				if (mat.spTextureMode == SphereMode::Mul)
					pixelConstants.textureModes.z = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					pixelConstants.textureModes.z = 2;
				sphereTexture = material.sphereTexture;
			}
			glBindTextureUnit(2, sphereTexture);
			if (!UpdateUniformBuffer(instance.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants)))
				continue;
			if (mat.bothFace) {
				if (cullEnabled) {
					glDisable(GL_CULL_FACE);
					cullEnabled = false;
				}
			} else {
				if (!cullEnabled) {
					glEnable(GL_CULL_FACE);
					cullEnabled = true;
				}
				if (cullFaceMode != GL_BACK) {
					glCullFace(GL_BACK);
					cullFaceMode = GL_BACK;
				}
			}
			const size_t offset = beginIndex * instance.model->geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void GlfwDrawer::DrawEdge() {
		const auto* viewer = instance.viewer;
		if (!viewer->edgeEffectEnabled)
			return;
		const auto& materials = instance.materials;
		const auto indexType = instance.indexType;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		const auto& edgeShader = viewer->edgeShader;
		EdgeVertexConstants baseVertexConstants{};
		baseVertexConstants.wv = view * world;
		baseVertexConstants.wvp = proj * view * world;
		baseVertexConstants.screenSize = glm::vec2(viewer->screenWidth, viewer->screenHeight);
		glUseProgram(edgeShader->program);
		glBindVertexArray(instance.edgeVao);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_POLYGON_OFFSET_FILL);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
		glCullFace(GL_FRONT);
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.edgeFlag)
				continue;
			if (mat.diffuse.a == 0.0f)
				continue;
			EdgeVertexConstants vertexConstants = baseVertexConstants;
			vertexConstants.edgeSize = mat.edgeSize;
			EdgePixelConstants pixelConstants;
			pixelConstants.edgeColor = mat.edgeColor;
			if (!UpdateUniformBuffer(instance.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)) ||
				!UpdateUniformBuffer(instance.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants)))
				continue;
			const size_t offset = beginIndex * instance.model->geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void GlfwDrawer::DrawGroundShadow() {
		const auto* viewer = instance.viewer;
		if (!viewer->groundShadowEffectEnabled)
			return;
		const auto& materials = instance.materials;
		const auto indexType = instance.indexType;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		const auto& gsShader = viewer->gsShader;
		glUseProgram(gsShader->program);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		const glm::mat4 shadow = BuildGroundShadowMatrix(viewer->lightDir);
		GroundShadowVertexConstants vertexConstants;
		vertexConstants.wvp = proj * view * shadow * world;
		if (!UpdateUniformBuffer(instance.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)))
			return;
		glBindVertexArray(instance.gsVao);
		constexpr GroundShadowPixelConstants pixelConstants;
		if (!UpdateUniformBuffer(instance.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants)))
			return;
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1, -1);
		if (pixelConstants.shadowColor.a < 1.0f) {
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
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow)
				continue;
			if (mat.diffuse.a == 0.0f)
				continue;
			const size_t offset = beginIndex * instance.model->geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
		glDisable(GL_POLYGON_OFFSET_FILL);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
	}

	void GlfwDrawer::DrawDepthOnly() {
		const auto* viewer = instance.viewer;
		const auto indexType = instance.indexType;
		const auto& view = viewer->viewMat;
		const auto& proj = viewer->projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(instance.scale));
		ModelVertexConstants vertexConstants;
		vertexConstants.wv = view * world;
		vertexConstants.wvp = proj * view * world;
		glUseProgram(viewer->depthOnlyShader->program);
		if (!UpdateUniformBuffer(instance.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)))
			return;
		glBindVertexArray(instance.vao);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_POLYGON_OFFSET_FILL);
		glFrontFace(GL_CCW);
		glCullFace(GL_BACK);
		glEnable(GL_CULL_FACE);
		bool cullEnabled = true;
		for (const auto& [beginIndex, indexCount, materialId] : instance.model->materialData.subMeshes) {
			const auto& mat = instance.materials[materialId].mat;
			if (mat.diffuse.a == 0.0f)
				continue;
			if (mat.bothFace) {
				if (cullEnabled) {
					glDisable(GL_CULL_FACE);
					cullEnabled = false;
				}
			} else if (!cullEnabled) {
				glEnable(GL_CULL_FACE);
				cullEnabled = true;
			}
			const size_t offset = beginIndex * instance.model->geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	GlfwDrawer::GlfwDrawer(GlfwInstance& sourceInstance) : instance(sourceInstance) {}
}
