#include "Viewer/Drawer/Drawer.h"

#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Viewer/Viewer.h"
#include "Core/Model/Model.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Chrivent {
	const glm::mat4& Drawer::ClipMatrix() const {
		static constexpr glm::mat4 clipMatrix(1.0f);
		return clipMatrix;
	}

	glm::mat4 Drawer::BuildGroundShadowMatrix(const glm::vec3& lightDir) {
		constexpr glm::vec4 plane(0.0f, 1.0f, 0.0f, 0.0f);
		const glm::vec4 light(-lightDir, 0.0f);
		return glm::dot(plane, light) * glm::mat4(1.0f) - glm::outerProduct(light, plane);
	}

	glm::mat4 Drawer::BuildWorldMatrix(const float scale) {
		return glm::scale(glm::mat4(1.0f), glm::vec3(scale));
	}

	ModelVertexConstants Drawer::BuildModelVertexConstants(
		const Viewer& viewer, const glm::mat4& world, const glm::mat4& clipMatrix) {
		return {
			.wv = viewer.viewMat * world,
			.wvp = clipMatrix * viewer.projMat * viewer.viewMat * world
		};
	}

	ModelPixelConstants Drawer::BuildModelPixelConstants(const Viewer& viewer, const Material& material,
		const int textureMode, const int toonTextureMode, const int sphereTextureMode) {
		return {
			.texMulFactor = material.textureMulFactor,
			.texAddFactor = material.textureAddFactor,
			.toonTexMulFactor = material.toonTextureMulFactor,
			.toonTexAddFactor = material.toonTextureAddFactor,
			.sphereTexMulFactor = material.sphereTextureMulFactor,
			.sphereTexAddFactor = material.sphereTextureAddFactor,
			.textureModes = glm::ivec4(textureMode, toonTextureMode, sphereTextureMode, 0),
			.diffuseAlpha = material.diffuse,
			.ambientSpecularPower = glm::vec4(material.ambient, material.specularPower),
			.specular = glm::vec4(material.specular, 0.0f),
			.lightColor = glm::vec4(viewer.lightColor, 0.0f),
			.lightDir = glm::vec4(glm::mat3(viewer.viewMat) * viewer.lightDir, 0.0f)
		};
	}

	EdgeVertexConstants Drawer::BuildEdgeVertexConstants(const Viewer& viewer, const glm::mat4& world,
		const glm::mat4& clipMatrix, const glm::vec2& screenSize) {
		return {
			.wv = viewer.viewMat * world,
			.wvp = clipMatrix * viewer.projMat * viewer.viewMat * world,
			.screenSize = screenSize
		};
	}

	GroundShadowVertexConstants Drawer::BuildGroundShadowVertexConstants(
		const Viewer& viewer, const glm::mat4& world, const glm::mat4& clipMatrix) {
		return { .wvp = clipMatrix * viewer.projMat * viewer.viewMat
			* BuildGroundShadowMatrix(viewer.lightDir) * world };
	}

	SceneVelocityVertexConstants Drawer::BuildSceneVelocityVertexConstants(
		const Viewer& viewer, const glm::mat4& world, const glm::mat4& clipMatrix) {
		SceneVelocityVertexConstants constants;
		constants.currentWvp = clipMatrix * viewer.projMat * viewer.viewMat * world;
		constants.previousWvp = viewer.postProcessHistoryResetPending ? constants.currentWvp
			: clipMatrix * viewer.previousProjMat * viewer.previousViewMat * world;
		return constants;
	}

	bool Drawer::ShouldDrawPostProcessSurface(const float opacity) {
		return opacity >= PostProcessInputLayout::surfaceOpacityThreshold;
	}

	SceneSurfacePixelConstants Drawer::BuildSceneSurfacePixelConstants(
		const float opacity, const bool textureHasAlpha) {
		return {
			.materialOpacity = opacity,
			.textureAlphaEnabled = textureHasAlpha ? 1.0f : 0.0f,
			.alphaCutoff = PostProcessInputLayout::surfaceOpacityThreshold
		};
	}

	Drawer::~Drawer() = default;

	void Drawer::Draw() {
		DrawModel();
		DrawEdge();
		DrawGroundShadow();
	}

	void Drawer::DrawPostProcessDepth() {
		DrawDepthOnly();
	}
}
