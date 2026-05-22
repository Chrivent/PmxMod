#include "GlfwDrawer.h"

#include "GlfwInstance.h"
#include "GlfwViewer.h"
#include "../../Model/Model.h"

namespace Chrivent {
	GlfwDrawer::GlfwDrawer(const GlfwInstanceInfo& sourceInfo) : info(sourceInfo) {}

	void GlfwDrawer::DrawModel() const {
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto vao = info.vao;
		const auto indexType = info.indexType;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		auto wv = view * world;
		auto wvp = proj * view * world;
		const auto& shader = viewer->GetGlfwInfo().shader;
		glm::vec3 lightColor = viewer->GetInfo().lightColor;
		glm::vec3 lightDir = glm::mat3(viewer->GetInfo().viewMat) * viewer->GetInfo().lightDir;
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
				glBindTexture(GL_TEXTURE_2D, viewer->GetGlfwInfo().dummyColorTex);
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
				glBindTexture(GL_TEXTURE_2D, viewer->GetGlfwInfo().dummyColorTex);
			}
			glActiveTexture(GL_TEXTURE0 + 2);
			if (material.toonTexture != 0) {
				glUniform4fv(shader->toonTexMulFactorLocation, 1, &mat.toonTextureMulFactor[0]);
				glUniform4fv(shader->toonTexAddFactorLocation, 1, &mat.toonTextureAddFactor[0]);
				glUniform1i(shader->toonTexModeLocation, 1);
				glBindTexture(GL_TEXTURE_2D, material.toonTexture);
			} else {
				glUniform1i(shader->toonTexModeLocation, 0);
				glBindTexture(GL_TEXTURE_2D, viewer->GetGlfwInfo().dummyColorTex);
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
		const auto* viewer = info.viewer;
		const auto& materials = info.materials;
		const auto edgeVao = info.edgeVao;
		const auto indexType = info.indexType;
		const auto& view = viewer->GetInfo().viewMat;
		const auto& proj = viewer->GetInfo().projMat;
		const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(info.scale));
		auto wv = view * world;
		auto wvp = proj * view * world;
		const auto& edgeShader = viewer->GetGlfwInfo().edgeShader;
		glUseProgram(edgeShader->program);
		glUniformMatrix4fv(edgeShader->wvLocation, 1, GL_FALSE, &wv[0][0]);
		glUniformMatrix4fv(edgeShader->wvpLocation, 1, GL_FALSE, &wvp[0][0]);
		glm::vec2 screenSize(viewer->GetInfo().screenWidth, viewer->GetInfo().screenHeight);
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
}
