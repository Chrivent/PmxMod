#include "Viewer/Drawer/OpenGlDrawer.h"

#include "Viewer/Instance/OpenGlInstance.h"
#include "Viewer/Viewer/OpenGlViewer.h"
#include "Core/Model/Model.h"
#include "Viewer/Shader/ShaderConstants.h"

namespace Chrivent {
	void OpenGlDrawer::BeginDrawFrame() {
		instance.vertexConstantsRing.BeginFrame(0);
		instance.pixelConstantsRing.BeginFrame(0);
	}

	bool OpenGlDrawer::UpdateUniformBuffer(OpenGlDynamicBufferRing& ring, const GLuint binding, const void* data, const size_t size) const {
		std::string error;
		const auto slice = ring.Allocate(size, instance.uniformBufferOffsetAlignment, error);
		if (!slice.has_value())
			return false;
		glNamedBufferSubData(ring.GetBuffer(), slice->offset, slice->size, data);
		glBindBufferRange(GL_UNIFORM_BUFFER, binding, ring.GetBuffer(), slice->offset, slice->size);
		return true;
	}

	void OpenGlDrawer::DrawModel() {
		const auto& viewer = renderer;
		const auto& materials = instance.materials;
		const auto indexType = instance.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
		const auto* shader = viewer.GetModelShader();
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
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			const int textureMode = material.texture == 0 ? 0 : material.textureHasAlpha ? 2 : 1;
			const int toonTextureMode = material.toonTexture != 0 ? 1 : 0;
			int sphereTextureMode = 0;
			if (material.sphereTexture != 0) {
				if (mat.spTextureMode == SphereMode::Mul)
					sphereTextureMode = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					sphereTextureMode = 2;
			}
			const ModelPixelConstants pixelConstants = BuildModelPixelConstants(
				viewer, mat, textureMode, toonTextureMode, sphereTextureMode);
			GLuint baseTexture = viewer.GetDummyColorTexture();
			if (material.texture != 0)
				baseTexture = material.texture;
			glBindTextureUnit(0, baseTexture);
			GLuint toonTexture = viewer.GetDummyColorTexture();
			if (material.toonTexture != 0)
				toonTexture = material.toonTexture;
			glBindTextureUnit(1, toonTexture);
			GLuint sphereTexture = viewer.GetDummyColorTexture();
			if (material.sphereTexture != 0)
				sphereTexture = material.sphereTexture;
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
			const size_t offset = beginIndex * instance.GetModel().geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void OpenGlDrawer::DrawEdge() {
		const auto& viewer = renderer;
		const auto& materials = instance.materials;
		const auto indexType = instance.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const auto* edgeShader = viewer.GetEdgeShader();
		const EdgeVertexConstants baseVertexConstants = BuildEdgeVertexConstants(
			viewer, world, ClipMatrix(), glm::vec2(viewer.screenWidth, viewer.screenHeight));
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
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
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
			const size_t offset = beginIndex * instance.GetModel().geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void OpenGlDrawer::DrawGroundShadow() {
		const auto& viewer = renderer;
		const auto& materials = instance.materials;
		const auto indexType = instance.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const auto* gsShader = viewer.GetGroundShadowShader();
		glUseProgram(gsShader->program);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		const GroundShadowVertexConstants vertexConstants = BuildGroundShadowVertexConstants(
			viewer, world, ClipMatrix());
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
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (!mat.groundShadow)
				continue;
			if (mat.diffuse.a == 0.0f)
				continue;
			const size_t offset = beginIndex * instance.GetModel().geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
		glDisable(GL_POLYGON_OFFSET_FILL);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
	}

	void OpenGlDrawer::DrawSceneInputs() {
		const auto& viewer = renderer;
		const auto indexType = instance.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		if (viewer.RequiresPostProcessVelocity()) {
			const SceneVelocityVertexConstants vertexConstants = BuildSceneVelocityVertexConstants(
				viewer, world, ClipMatrix());
			glUseProgram(viewer.GetSceneVelocityShader()->program);
			if (!UpdateUniformBuffer(instance.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)))
				return;
			glBindVertexArray(instance.velocityVao);
		} else {
			const ModelVertexConstants vertexConstants = BuildModelVertexConstants(viewer, world, ClipMatrix());
			glUseProgram(viewer.GetDepthOnlyShader()->program);
			if (!UpdateUniformBuffer(instance.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants)))
				return;
			glBindVertexArray(instance.depthVao);
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
			const auto& material = instance.materials[materialId];
			const auto& mat = material.mat;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.texture != 0 && material.textureHasAlpha);
			if (!UpdateUniformBuffer(instance.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants)))
				continue;
			glBindTextureUnit(0, material.texture != 0 ? material.texture : viewer.GetDummyColorTexture());
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

	OpenGlDrawer::OpenGlDrawer(OpenGlInstance& sourceInstance, OpenGlViewer& sourceViewer)
		: Drawer(sourceViewer), instance(sourceInstance), renderer(sourceViewer) {}
}
