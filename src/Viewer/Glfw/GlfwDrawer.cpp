#include "GlfwDrawer.h"

#include "GlfwInstance.h"
#include "GlfwViewer.h"
#include "../../Model/Model.h"
#include "../Assist/Glsl/GlslShaderConstants.h"

namespace Chrivent {
	void GlfwDrawer::UpdateUniformBuffer(const GLuint buffer, const GLuint binding, const void* data, const size_t size) {
		glBindBuffer(GL_UNIFORM_BUFFER, buffer);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, buffer);
	}

	void GlfwDrawer::DrawModel() const {
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto vao = info.vao;
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
		glUseProgram(shader->program);
		UpdateUniformBuffer(info.vertexConstantsUbo, 0, &vertexConstants, sizeof(vertexConstants));
		glBindVertexArray(vao);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		for (const auto& [beginIndex, indexCount, materialId] : info.model->materialData.subMeshes) {
			const auto& material = materials[materialId];
			const auto& mat = material.mat;
			if (mat.diffuse.a == 0)
				continue;
			ModelPixelConstants pixelConstants{};
			pixelConstants.diffuseAlpha = glm::vec4(mat.diffuse.r, mat.diffuse.g, mat.diffuse.b, mat.diffuse.a);
			pixelConstants.ambientSpecularPower = glm::vec4(mat.ambient, mat.specularPower);
			pixelConstants.specular = glm::vec4(mat.specular, 0.0f);
			pixelConstants.lightColor = glm::vec4(lightColor, 0.0f);
			pixelConstants.lightDir = glm::vec4(lightDir, 0.0f);
			pixelConstants.texMulFactor = mat.textureMulFactor;
			pixelConstants.texAddFactor = mat.textureAddFactor;
			pixelConstants.toonTexMulFactor = mat.toonTextureMulFactor;
			pixelConstants.toonTexAddFactor = mat.toonTextureAddFactor;
			pixelConstants.sphereTexMulFactor = mat.sphereTextureMulFactor;
			pixelConstants.sphereTexAddFactor = mat.sphereTextureAddFactor;
			glActiveTexture(GL_TEXTURE0 + 0);
			if (material.texture != 0) {
				if (!material.textureHasAlpha)
					pixelConstants.textureModes.x = 1;
				else
					pixelConstants.textureModes.x = 2;
				glBindTexture(GL_TEXTURE_2D, material.texture);
			} else
				glBindTexture(GL_TEXTURE_2D, viewer->GetGlfwInfo().dummyColorTex);
			glActiveTexture(GL_TEXTURE0 + 1);
			if (material.toonTexture != 0) {
				pixelConstants.textureModes.y = 1;
				glBindTexture(GL_TEXTURE_2D, material.toonTexture);
			} else
				glBindTexture(GL_TEXTURE_2D, viewer->GetGlfwInfo().dummyColorTex);
			glActiveTexture(GL_TEXTURE0 + 2);
			if (material.sphereTexture != 0) {
				if (mat.spTextureMode == SphereMode::Mul)
					pixelConstants.textureModes.z = 1;
				else if (mat.spTextureMode == SphereMode::Add)
					pixelConstants.textureModes.z = 2;
				glBindTexture(GL_TEXTURE_2D, material.sphereTexture);
			} else
				glBindTexture(GL_TEXTURE_2D, viewer->GetGlfwInfo().dummyColorTex);
			UpdateUniformBuffer(info.pixelConstantsUbo, 1, &pixelConstants, sizeof(pixelConstants));
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
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto edgeVao = info.edgeVao;
		const auto indexType = info.indexType;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		const auto& edgeShader = viewer->GetGlfwInfo().edgeShader;
		glUseProgram(edgeShader->program);
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
			EdgeVertexConstants vertexConstants;
			vertexConstants.wv = view * world;
			vertexConstants.wvp = proj * view * world;
			vertexConstants.screenSize = glm::vec2(viewer->GetInfo().screenWidth, viewer->GetInfo().screenHeight);
			vertexConstants.edgeSize = mat.edgeSize;
			EdgePixelConstants pixelConstants;
			pixelConstants.edgeColor = mat.edgeColor;
			UpdateUniformBuffer(info.vertexConstantsUbo, 0, &vertexConstants, sizeof(vertexConstants));
			UpdateUniformBuffer(info.pixelConstantsUbo, 1, &pixelConstants, sizeof(pixelConstants));
			const size_t offset = beginIndex * info.model->geometryData.indexElementSize;
			glDrawElements(GL_TRIANGLES, indexCount, indexType, reinterpret_cast<GLvoid*>(offset));
		}
	}

	void GlfwDrawer::DrawGroundShadow() const {
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto gsVao = info.gsVao;
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
		UpdateUniformBuffer(info.vertexConstantsUbo, 0, &vertexConstants, sizeof(vertexConstants));
		glBindVertexArray(gsVao);
		GroundShadowPixelConstants pixelConstants;
		const auto shadowColor = pixelConstants.shadowColor;
		UpdateUniformBuffer(info.pixelConstantsUbo, 1, &pixelConstants, sizeof(pixelConstants));
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

	GlfwDrawer::GlfwDrawer(const GlfwInstanceInfo& sourceInfo) : info(sourceInfo) {}
}
