#include "GlfwDrawer.h"

#include "GlfwInstance.h"
#include "GlfwViewer.h"
#include "../../../Model/Model.h"
#include "../GlslShaderConstants.h"

namespace Chrivent {
	void GlfwDrawer::BeginDynamicBufferFrame() const {
		info.vertexConstantsRing.BeginFrame(0);
		info.pixelConstantsRing.BeginFrame(0);
	}

	void GlfwDrawer::UpdateUniformBuffer(
		GlfwDynamicBufferRing& ring,
		const GLuint binding,
		const void* data,
		const size_t size) const {
		std::string error;
		const auto slice = ring.Allocate(size, info.uniformBufferOffsetAlignment, error);
		if (!slice.has_value())
			return;
		glBindBuffer(GL_UNIFORM_BUFFER, ring.GetBuffer());
		glBufferSubData(GL_UNIFORM_BUFFER, slice->offset, slice->size, data);
		glBindBufferRange(GL_UNIFORM_BUFFER, binding, ring.GetBuffer(), slice->offset, slice->size);
	}

	void GlfwDrawer::DrawModel() {
		BeginDynamicBufferFrame();
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto indexType = info.indexType;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		ModelVertexConstants vertexConstants;
		vertexConstants.wv = view * world;
		vertexConstants.wvp = proj * view * world;
		const auto& shader = viewer->GetGlfwInfo().shader;
		const glm::vec3 lightColor = viewer->GetInfo().lightColor;
		const glm::vec3 lightDir = glm::mat3(viewer->GetInfo().viewMat) * viewer->GetInfo().lightDir;
		ModelPixelConstants basePixelConstants{};
		basePixelConstants.lightColor = glm::vec4(lightColor, 0.0f);
		basePixelConstants.lightDir = glm::vec4(lightDir, 0.0f);
		glUseProgram(shader->program);
		UpdateUniformBuffer(
			info.vertexConstantsRing,
			0,
			&vertexConstants,
			sizeof(vertexConstants));
		glBindVertexArray(info.vao);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		GLuint boundTextures[3] = { 0, 0, 0 };
		bool cullEnabled = true;
		GLenum cullFaceMode = GL_BACK;
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			ModelPixelConstants pixelConstants = basePixelConstants;
			pixelConstants.diffuseAlpha = glm::vec4(mat.diffuse.r, mat.diffuse.g, mat.diffuse.b, mat.diffuse.a);
			pixelConstants.ambientSpecularPower = glm::vec4(mat.ambient, mat.specularPower);
			pixelConstants.specular = glm::vec4(mat.specular, 0.0f);
			pixelConstants.texMulFactor = mat.textureMulFactor;
			pixelConstants.texAddFactor = mat.textureAddFactor;
			pixelConstants.toonTexMulFactor = mat.toonTextureMulFactor;
			pixelConstants.toonTexAddFactor = mat.toonTextureAddFactor;
			pixelConstants.sphereTexMulFactor = mat.sphereTextureMulFactor;
			pixelConstants.sphereTexAddFactor = mat.sphereTextureAddFactor;
			glActiveTexture(GL_TEXTURE0 + 0);
			GLuint baseTexture = viewer->GetGlfwInfo().dummyColorTex;
			if (material.texture != 0) {
				if (!material.textureHasAlpha)
					pixelConstants.textureModes.x = 1;
				else
					pixelConstants.textureModes.x = 2;
				baseTexture = material.texture;
			}
			if (boundTextures[0] != baseTexture) {
				glBindTexture(GL_TEXTURE_2D, baseTexture);
				boundTextures[0] = baseTexture;
			}
			glActiveTexture(GL_TEXTURE0 + 1);
			GLuint toonTexture = viewer->GetGlfwInfo().dummyColorTex;
			if (material.toonTexture != 0) {
				pixelConstants.textureModes.y = 1;
				toonTexture = material.toonTexture;
			}
			if (boundTextures[1] != toonTexture) {
				glBindTexture(GL_TEXTURE_2D, toonTexture);
				boundTextures[1] = toonTexture;
			}
			glActiveTexture(GL_TEXTURE0 + 2);
			GLuint sphereTexture = viewer->GetGlfwInfo().dummyColorTex;
			if (material.sphereTexture != 0) {
				if (mat.spTextureMode == SphereMode::Mul)
					pixelConstants.textureModes.z = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					pixelConstants.textureModes.z = 2;
				sphereTexture = material.sphereTexture;
			}
			if (boundTextures[2] != sphereTexture) {
				glBindTexture(GL_TEXTURE_2D, sphereTexture);
				boundTextures[2] = sphereTexture;
			}
			UpdateUniformBuffer(
				info.pixelConstantsRing,
				1,
				&pixelConstants,
				sizeof(pixelConstants));
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
			const size_t offset = beginIndex * info.model->geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void GlfwDrawer::DrawEdge() {
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto indexType = info.indexType;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const auto& edgeShader = viewer->GetGlfwInfo().edgeShader;
		EdgeVertexConstants baseVertexConstants{};
		baseVertexConstants.wv = view * world;
		baseVertexConstants.wvp = proj * view * world;
		baseVertexConstants.screenSize = glm::vec2(viewer->GetInfo().screenWidth, viewer->GetInfo().screenHeight);
		glUseProgram(edgeShader->program);
		glBindVertexArray(info.edgeVao);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
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
			UpdateUniformBuffer(
				info.vertexConstantsRing,
				0,
				&vertexConstants,
				sizeof(vertexConstants));
			UpdateUniformBuffer(
				info.pixelConstantsRing,
				1,
				&pixelConstants,
				sizeof(pixelConstants));
			const size_t offset = beginIndex * info.model->geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void GlfwDrawer::DrawGroundShadow() {
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto indexType = info.indexType;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const auto& gsShader = viewer->GetGlfwInfo().gsShader;
		glUseProgram(gsShader->program);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1, -1);
		constexpr glm::vec4 plane(0.f, 1.f, 0.f, 0.f);
		const glm::vec4 light(-viewer->GetInfo().lightDir, 0.f);
		const glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.0f) - glm::outerProduct(light, plane);
		GroundShadowVertexConstants vertexConstants;
		vertexConstants.wvp = proj * view * shadow * world;
		UpdateUniformBuffer(
			info.vertexConstantsRing,
			0,
			&vertexConstants,
			sizeof(vertexConstants));
		glBindVertexArray(info.gsVao);
		constexpr GroundShadowPixelConstants pixelConstants;
		UpdateUniformBuffer(
			info.pixelConstantsRing,
			1,
			&pixelConstants,
			sizeof(pixelConstants));
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

	GlfwDrawer::GlfwDrawer(GlfwInstanceInfo& sourceInfo) : info(sourceInfo) {}
}
