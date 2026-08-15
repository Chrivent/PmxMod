#include "Viewer/Drawer/OpenGlDrawer.h"

#include "Viewer/DrawContext/OpenGlDrawContext.h"
#include "Viewer/Error/OpenGlErrorState.h"
#include "Viewer/Instance/OpenGlInstance.h"
#include "Core/Model/Model.h"
#include "Viewer/Shader/ShaderConstants.h"

namespace Chrivent {
	void OpenGlDrawer::BeginDrawFrame() {
		resources.vertexConstantsRing.BeginFrame(0);
		resources.pixelConstantsRing.BeginFrame(0);
	}

	DynamicBufferError::Result<void> OpenGlDrawer::UpdateUniformBuffer(
		OpenGlDynamicBufferRing& ring, const GLuint binding, const void* data, const size_t size) const {
		const auto sliceResult = ring.Allocate(size, resources.uniformBufferOffsetAlignment);
		if (!sliceResult)
			return std::unexpected(sliceResult.error());
		const UploadSlice& slice = *sliceResult;
		glNamedBufferSubData(ring.GetBuffer(), slice.offset, slice.size, data);
		glBindBufferRange(GL_UNIFORM_BUFFER, binding, ring.GetBuffer(), slice.offset, slice.size);
		return {};
	}

	GraphicsError::Result<void> OpenGlDrawer::DrawModel() {
		OpenGlErrorState::Clear();
		const auto& materials = resources.materials;
		const auto indexType = resources.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const ModelVertexConstants vertexConstants = BuildModelVertexConstants(drawState, world, ClipMatrix());
		drawContext.BindModelPipeline();
		const auto vertexUploadResult = UpdateUniformBuffer(
			resources.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants));
		if (!vertexUploadResult) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"OpenGL 모델 패스 기록", vertexUploadResult.error().message));
		}
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
		const GLuint dummyColorTexture = drawContext.GetDummyColorTexture();
		GLuint boundTextures[3]{};
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
				drawState, mat, base, toon, sphere);
			GLuint baseTexture = dummyColorTexture;
			if (material.texture != 0)
				baseTexture = material.texture;
			if (boundTextures[0] != baseTexture) {
				glBindTextureUnit(0, baseTexture);
				boundTextures[0] = baseTexture;
			}
			GLuint toonTexture = dummyColorTexture;
			if (material.toonTexture != 0)
				toonTexture = material.toonTexture;
			if (boundTextures[1] != toonTexture) {
				glBindTextureUnit(1, toonTexture);
				boundTextures[1] = toonTexture;
			}
			GLuint sphereTexture = dummyColorTexture;
			if (material.sphereTexture != 0)
				sphereTexture = material.sphereTexture;
			if (boundTextures[2] != sphereTexture) {
				glBindTextureUnit(2, sphereTexture);
				boundTextures[2] = sphereTexture;
			}
			const auto pixelUploadResult = UpdateUniformBuffer(
				resources.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants));
			if (!pixelUploadResult) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"OpenGL 모델 패스 기록", pixelUploadResult.error().message));
			}
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
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), indexType,
				reinterpret_cast<GLvoid*>(offset));
		}
		return OpenGlErrorState::ResolveResult(GraphicsErrorCode::CommandRecordingFailed,
			"OpenGL 모델 패스 기록", "OpenGL 모델 draw 명령을 기록하지 못했습니다");
	}

	GraphicsError::Result<void> OpenGlDrawer::DrawEdge() {
		OpenGlErrorState::Clear();
		const auto& materials = resources.materials;
		const auto indexType = resources.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		const EdgeVertexConstants baseVertexConstants = BuildEdgeVertexConstants(
			drawState, world, ClipMatrix(), drawState.screenSize);
		drawContext.BindEdgePipeline();
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
			const auto vertexUploadResult = UpdateUniformBuffer(
				resources.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants));
			if (!vertexUploadResult) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"OpenGL 엣지 패스 기록", vertexUploadResult.error().message));
			}
			const auto pixelUploadResult = UpdateUniformBuffer(
				resources.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants));
			if (!pixelUploadResult) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"OpenGL 엣지 패스 기록", pixelUploadResult.error().message));
			}
			const size_t offset = beginIndex * instance.GetModel().geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), indexType,
				reinterpret_cast<GLvoid*>(offset));
		}
		return OpenGlErrorState::ResolveResult(GraphicsErrorCode::CommandRecordingFailed,
			"OpenGL 엣지 패스 기록", "OpenGL 엣지 draw 명령을 기록하지 못했습니다");
	}

	GraphicsError::Result<void> OpenGlDrawer::DrawGroundShadow() {
		OpenGlErrorState::Clear();
		const auto& materials = resources.materials;
		const auto indexType = resources.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		drawContext.BindGroundShadowPipeline();
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		const GroundShadowVertexConstants vertexConstants = BuildGroundShadowVertexConstants(
			drawState, world, ClipMatrix());
		const auto vertexUploadResult = UpdateUniformBuffer(
			resources.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants));
		if (!vertexUploadResult) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"OpenGL 지면 그림자 패스 기록", vertexUploadResult.error().message));
		}
		glBindVertexArray(resources.gsVao);
		constexpr GroundShadowPixelConstants pixelConstants;
		const auto pixelUploadResult = UpdateUniformBuffer(
			resources.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants));
		if (!pixelUploadResult) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"OpenGL 지면 그림자 패스 기록", pixelUploadResult.error().message));
		}
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
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), indexType,
				reinterpret_cast<GLvoid*>(offset));
		}
		glDisable(GL_POLYGON_OFFSET_FILL);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
		return OpenGlErrorState::ResolveResult(GraphicsErrorCode::CommandRecordingFailed,
			"OpenGL 지면 그림자 패스 기록", "OpenGL 지면 그림자 draw 명령을 기록하지 못했습니다");
	}

	GraphicsError::Result<void> OpenGlDrawer::DrawSceneInputs() {
		OpenGlErrorState::Clear();
		const auto indexType = resources.indexType;
		const auto world = BuildWorldMatrix(instance.GetScale());
		if (drawState.velocityRequired) {
			const SceneVelocityVertexConstants vertexConstants = BuildSceneVelocityVertexConstants(
				drawState, world, ClipMatrix());
			drawContext.BindSceneVelocityPipeline();
			const auto uploadResult = UpdateUniformBuffer(
				resources.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants));
			if (!uploadResult) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"OpenGL 후처리 장면 입력 기록", uploadResult.error().message));
			}
			glBindVertexArray(resources.velocityVao);
		} else {
			const ModelVertexConstants vertexConstants = BuildModelVertexConstants(
				drawState, world, ClipMatrix());
			drawContext.BindSceneDepthPipeline();
			const auto uploadResult = UpdateUniformBuffer(
				resources.vertexConstantsRing, 0, &vertexConstants, sizeof(vertexConstants));
			if (!uploadResult) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"OpenGL 후처리 장면 입력 기록", uploadResult.error().message));
			}
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
		const GLuint dummyColorTexture = drawContext.GetDummyColorTexture();
		GLuint boundTexture = 0;
		bool cullEnabled = true;
		for (const auto& [beginIndex, indexCount, materialId] : instance.GetModel().materialData.subMeshes) {
			const auto& material = resources.materials[materialId];
			const auto& mat = material.material;
			if (!ShouldDrawPostProcessSurface(mat.diffuse.a))
				continue;
			const SceneSurfacePixelConstants pixelConstants = BuildSceneSurfacePixelConstants(
				mat.diffuse.a, material.texture != 0 && material.textureHasAlpha);
			const auto uploadResult = UpdateUniformBuffer(
				resources.pixelConstantsRing, 1, &pixelConstants, sizeof(pixelConstants));
			if (!uploadResult) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"OpenGL 후처리 장면 입력 기록", uploadResult.error().message));
			}
			const GLuint texture = material.texture != 0 ? material.texture : dummyColorTexture;
			if (boundTexture != texture) {
				glBindTextureUnit(0, texture);
				boundTexture = texture;
			}
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
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), indexType,
				reinterpret_cast<GLvoid*>(offset));
		}
		return OpenGlErrorState::ResolveResult(GraphicsErrorCode::CommandRecordingFailed,
			"OpenGL 후처리 장면 입력 기록", "OpenGL 장면 입력 draw 명령을 기록하지 못했습니다");
	}

	OpenGlDrawer::OpenGlDrawer(const OpenGlInstance& sourceInstance, OpenGlModelResources& sourceResources,
		OpenGlDrawContext& sourceDrawContext)
		: Drawer(GraphicsApi::OpenGl), instance(sourceInstance),
		resources(sourceResources), drawContext(sourceDrawContext) {}
}
