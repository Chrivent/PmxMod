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
		const SceneRenderState& scene = viewer.GetSceneRenderState();
		return {
			.wv = scene.viewMatrix * world,
			.wvp = clipMatrix * scene.projectionMatrix * scene.viewMatrix * world
		};
	}

	ModelPixelConstants Drawer::BuildModelPixelConstants(const Viewer& viewer, const Material& material,
		const int textureMode, const int toonTextureMode, const int sphereTextureMode) {
		const SceneRenderState& scene = viewer.GetSceneRenderState();
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
			.lightColor = glm::vec4(scene.lightColor, 0.0f),
			.lightDir = glm::vec4(glm::mat3(scene.viewMatrix) * scene.lightDirection, 0.0f)
		};
	}

	Drawer::MaterialTextureModes Drawer::ResolveMaterialTextureModes(const Material& material,
		const bool baseAvailable, const bool baseHasAlpha, const bool toonAvailable,
		const bool sphereAvailable) {
		MaterialTextureModes modes;
		modes.base = baseAvailable ? baseHasAlpha ? 2 : 1 : 0;
		modes.toon = toonAvailable ? 1 : 0;
		if (sphereAvailable) {
			if (material.spTextureMode == SphereMode::Mul)
				modes.sphere = 1;
			else if (material.spTextureMode == SphereMode::Add)
				modes.sphere = 2;
		}
		return modes;
	}

	bool Drawer::ShouldDrawModelMaterial(const Material& material) {
		return material.diffuse.a != 0.0f;
	}

	bool Drawer::ShouldDrawEdgeMaterial(const Material& material) {
		return material.edgeFlag && ShouldDrawModelMaterial(material);
	}

	bool Drawer::ShouldDrawGroundShadowMaterial(const Material& material) {
		return material.groundShadow && ShouldDrawModelMaterial(material);
	}

	EdgeVertexConstants Drawer::BuildEdgeVertexConstants(const Viewer& viewer, const glm::mat4& world,
		const glm::mat4& clipMatrix, const glm::vec2& screenSize) {
		const SceneRenderState& scene = viewer.GetSceneRenderState();
		return {
			.wv = scene.viewMatrix * world,
			.wvp = clipMatrix * scene.projectionMatrix * scene.viewMatrix * world,
			.screenSize = screenSize
		};
	}

	GroundShadowVertexConstants Drawer::BuildGroundShadowVertexConstants(
		const Viewer& viewer, const glm::mat4& world, const glm::mat4& clipMatrix) {
		const SceneRenderState& scene = viewer.GetSceneRenderState();
		return { .wvp = clipMatrix * scene.projectionMatrix * scene.viewMatrix
			* BuildGroundShadowMatrix(scene.lightDirection) * world };
	}

	SceneVelocityVertexConstants Drawer::BuildSceneVelocityVertexConstants(
		const Viewer& viewer, const glm::mat4& world, const glm::mat4& clipMatrix) {
		const SceneRenderState& scene = viewer.GetSceneRenderState();
		SceneVelocityVertexConstants constants;
		constants.currentWvp = clipMatrix * scene.projectionMatrix * scene.viewMatrix * world;
		constants.previousWvp = viewer.IsPostProcessHistoryResetPending() ? constants.currentWvp
			: clipMatrix * viewer.GetPreviousProjectionMatrix() * viewer.GetPreviousViewMatrix() * world;
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

	void Drawer::BeginDraw() {
		BeginDrawFrame();
	}

	void Drawer::DrawModelPass() {
		if (viewer.GetSceneRenderState().modelEnabled)
			DrawModel();
	}

	void Drawer::DrawEdgePass() {
		if (viewer.GetSceneRenderState().edgeEnabled)
			DrawEdge();
	}

	void Drawer::DrawGroundShadowPass() {
		if (viewer.GetSceneRenderState().groundShadowEnabled)
			DrawGroundShadow();
	}

	void Drawer::DrawPostProcessSceneInputs() {
		DrawSceneInputs();
	}
}
