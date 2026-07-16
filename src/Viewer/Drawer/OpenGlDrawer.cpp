#include "Viewer/Drawer/OpenGlDrawer.h"

#include "Viewer/DrawContext/OpenGlDrawContext.h"
#include "Viewer/Instance/OpenGlInstance.h"
#include "Viewer/Viewer/Viewer.h"
#include "Core/Model/Model.h"
#include "Viewer/Shader/ShaderConstants.h"

#include <iostream>

namespace Chrivent {
	void OpenGlDrawer::BeginDrawFrame() {
		resources.vertexConstantsRing.BeginFrame(0);
		resources.pixelConstantsRing.BeginFrame(0);
	}

	bool OpenGlDrawer::UpdateUniformBuffer(OpenGlDynamicBufferRing& ring, const GLuint binding, const void* data, const size_t size) const {
		std::string error;
		const auto slice = ring.Allocate(size, resources.uniformBufferOffsetAlignment, error);
		if (!slice.has_value()) {
			std::cerr << error << '\n';
			return false;
		}
		glNamedBufferSubData(ring.GetBuffer(), slice->offset, slice->size, data);
		glBindBufferRange(GL_UNIFORM_BUFFER, binding, ring.GetBuffer(), slice->offset, slice->size);
		return true;
	}

	void OpenGlDrawer::DrawModel() {
		const auto& viewer = this->viewer;
		const auto& materials = resources.materials;
		const auto indexType = resources.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		const auto& shader = drawContext.GetModelShader();
		glUseProgram(shader.program);
		if (!UpdateUniformBuffer(resources.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)))
			return;
		glBindVertexArray(resources.vao);
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
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawModelMaterial(mat))
				continue;
			const auto [base, toon, sphere] = ResolveMaterialTextureModes(mat,
				material.texture != 0, material.textureHasAlpha,
				material.toonTexture != 0, material.sphereTexture != 0);
			const ModelPixelConstants pixelConstants = BuildModelPixelConstants(
				viewer, mat, base, toon, sphere);
			GLuint baseTexture = drawContext.GetDummyColorTexture();
			if (material.texture != 0)
				baseTexture = material.texture;
			glBindTextureUnit(0, baseTexture);
			GLuint toonTexture = drawContext.GetDummyColorTexture();
			if (material.toonTexture != 0)
				toonTexture = material.toonTexture;
			glBindTextureUnit(1, toonTexture);
			GLuint sphereTexture = drawContext.GetDummyColorTexture();
			if (material.sphereTexture != 0)
				sphereTexture = material.sphereTexture;
			glBindTextureUnit(2, sphereTexture);
			if (!UpdateUniformBuffer(resources.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants)))
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
			const size_t offset = beginIndex * instance.GetModel().geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void OpenGlDrawer::DrawEdge() {
		const auto& viewer = this->viewer;
		const auto& materials = resources.materials;
		const auto indexType = resources.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const auto& edgeShader = drawContext.GetEdgeShader();
		const EdgeVertexConstants baseVertexConstants = BuildEdgeVertexConstants(
			viewer, world, ClipMatrix(), glm::vec2(viewer.GetScreenWidth(), viewer.GetScreenHeight()));
		glUseProgram(edgeShader.program);
		glBindVertexArray(resources.edgeVao);
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
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawEdgeMaterial(mat))
				continue;
			EdgeVertexConstants vertexConstants = baseVertexConstants;
			vertexConstants.edgeSize = mat.edgeSize;
			EdgePixelConstants pixelConstants;
			pixelConstants.edgeColor = mat.edgeColor;
			if (!UpdateUniformBuffer(resources.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)) ||
				!UpdateUniformBuffer(resources.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants)))
				continue;
			const size_t offset = beginIndex * instance.GetModel().geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void OpenGlDrawer::DrawGroundShadow() {
		const auto& viewer = this->viewer;
		const auto& materials = resources.materials;
		const auto indexType = resources.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const auto& groundShadowShader = drawContext.GetGroundShadowShader();
		glUseProgram(groundShadowShader.program);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		const GroundShadowVertexConstants vertexConstants = BuildGroundShadowVertexConstants(
			viewer, world, ClipMatrix());
		if (!UpdateUniformBuffer(resources.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)))
			return;
		glBindVertexArray(resources.gsVao);
		constexpr GroundShadowPixelConstants pixelConstants;
		if (!UpdateUniformBuffer(resources.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants)))
			return;
		glDepthMask(GL_FALSE);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1, -1);
		if (pixelConstants.shadowColor.a < 1.0f) {
			glEnable(GL_BLEND);
			glEnable(GL_STENCIL_TEST);
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
			glStencilFuncSeparate(GL_FRONT_AND_BACK, GL_NOTEQUAL, 1, 1);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		} else {
			glDisable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
		}
		glDisable(GL_CULL_FACE);
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawGroundShadowMaterial(mat))
				continue;
			const size_t offset = beginIndex * instance.GetModel().geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
		glDisable(GL_POLYGON_OFFSET_FILL);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}

	void OpenGlDrawer::DrawSceneInputs() {
		const auto& viewer = this->viewer;
		const auto indexType = resources.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		if (viewer.RequiresPostProcessVelocity()) {
			const SceneVelocityVertexConstants vertexConstants = BuildSceneVelocityVertexConstants(
				viewer, world, ClipMatrix());
			glUseProgram(drawContext.GetSceneVelocityShader().program);
			if (!UpdateUniformBuffer(resources.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)))
				return;
			glBindVertexArray(resources.velocityVao);
		} else {
			const ModelVertexConstants vertexConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
			glUseProgram(drawContext.GetDepthOnlyShader().program);
			if (!UpdateUniformBuffer(resources.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)))
				return;
			glBindVertexArray(resources.depthVao);
		}
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
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.texture != 0 && material.textureHasAlpha);
			if (!UpdateUniformBuffer(resources.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants)))
				continue;
			glBindTextureUnit(0, material.texture != 0
				? material.texture : drawContext.GetDummyColorTexture());
			if (mat.bothFace) {
				if (cullEnabled) {
					glDisable(GL_CULL_FACE);
					cullEnabled = false;
				}
			} else if (!cullEnabled) {
				glEnable(GL_CULL_FACE);
				cullEnabled = true;
			}
			const size_t offset = beginIndex * instance.GetModel().geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	OpenGlDrawer::OpenGlDrawer(const OpenGlInstance& sourceInstance, OpenGlModelResources& sourceResources,
		const OpenGlDrawContext& sourceDrawContext, Viewer& sourceViewer)
		: Drawer(sourceViewer), instance(sourceInstance), resources(sourceResources),
		drawContext(sourceDrawContext) {}
}
