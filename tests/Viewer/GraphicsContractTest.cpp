#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Command/Dx12CommandContext.h"
#include "Viewer/Drawer/Drawer.h"
#include "Viewer/PostProcess/PostProcessFrameData.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Shader/SpirvBindingLayout.h"
#include "Viewer/Texture/Dx11TextureCache.h"
#include "Viewer/Texture/Dx12TextureCache.h"
#include "Viewer/Texture/OpenGlTextureCache.h"
#include "Viewer/Texture/VulkanTextureCache.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace Chrivent {
	// 공통 Drawer의 패스 선택과 재질 판정 규칙을 GPU 없이 노출한다.
	class DrawerContractTestAdapter final : public Drawer {
		int beginCallCount = 0;
		int modelCallCount = 0;
		int edgeCallCount = 0;
		int groundShadowCallCount = 0;
		int sceneInputCallCount = 0;

	protected:
		// 프레임별 드로어 준비 호출을 기록한다.
		void BeginDrawFrame() override { beginCallCount++; }
		// 모델 패스 호출을 기록한다.
		GraphicsError::Result<void> DrawModel() override {
			modelCallCount++;
			return {};
		}
		// 엣지 패스 호출을 기록한다.
		GraphicsError::Result<void> DrawEdge() override {
			edgeCallCount++;
			return {};
		}
		// 지면 그림자 패스 호출을 기록한다.
		GraphicsError::Result<void> DrawGroundShadow() override {
			groundShadowCallCount++;
			return {};
		}
		// 후처리 장면 입력 패스 호출을 기록한다.
		GraphicsError::Result<void> DrawSceneInputs() override {
			sceneInputCallCount++;
			return {};
		}

	public:
		DrawerContractTestAdapter() : Drawer(GraphicsApi::Unknown) {}

		int GetBeginCallCount() const { return beginCallCount; }
		int GetModelCallCount() const { return modelCallCount; }
		int GetEdgeCallCount() const { return edgeCallCount; }
		int GetGroundShadowCallCount() const { return groundShadowCallCount; }
		int GetSceneInputCallCount() const { return sceneInputCallCount; }

		// 재질과 텍스처 보유 상태를 공통 셰이더 모드 값으로 변환한다.
		static glm::ivec3 ResolveTextureModes(const Material& material,
			const bool baseAvailable, const bool baseHasAlpha,
			const bool toonAvailable, const bool sphereAvailable) {
			const MaterialTextureModes modes = ResolveMaterialTextureModes(material,
				baseAvailable, baseHasAlpha, toonAvailable, sphereAvailable);
			return { modes.base, modes.toon, modes.sphere };
		}
		// 모델 패스에서 재질을 그릴지 반환한다.
		static bool IsModelMaterialVisible(const Material& material) {
			return ShouldDrawModelMaterial(material);
		}
		// 엣지 패스에서 재질을 그릴지 반환한다.
		static bool IsEdgeMaterialVisible(const Material& material) {
			return ShouldDrawEdgeMaterial(material);
		}
		// 지면 그림자 패스에서 재질을 그릴지 반환한다.
		static bool IsGroundShadowMaterialVisible(const Material& material) {
			return ShouldDrawGroundShadowMaterial(material);
		}
	};

	TEST(GraphicsContract, CpuShaderDataMatchesHlslPacking) {
		EXPECT_EQ(sizeof(ModelVertexConstants), 128);
		EXPECT_EQ(sizeof(SceneVelocityVertexConstants), 128);
		EXPECT_EQ(sizeof(SceneSurfacePixelConstants), 16);
		EXPECT_EQ(sizeof(ModelPixelConstants), 192);
		EXPECT_EQ(sizeof(EdgeVertexConstants), 144);
		EXPECT_EQ(sizeof(EdgePixelConstants), 16);
		EXPECT_EQ(sizeof(GroundShadowVertexConstants), 64);
		EXPECT_EQ(sizeof(GroundShadowPixelConstants), 16);
		EXPECT_EQ(sizeof(PostProcessFrameData), 144);
	}

	TEST(GraphicsContract, ViewerVertexLayoutMatchesAllApiInputDescriptions) {
		EXPECT_EQ(offsetof(ViewerVertex, position), 0);
		EXPECT_EQ(offsetof(ViewerVertex, normal), 12);
		EXPECT_EQ(offsetof(ViewerVertex, uv), 24);
		EXPECT_EQ(offsetof(ViewerVertex, previousPosition), 32);
		EXPECT_EQ(sizeof(ViewerVertex), 44);
	}

	TEST(GraphicsContract, SpirvBindingsKeepTexturesAndSamplersDisjoint) {
		EXPECT_EQ(SpirvBindingLayout::ResolveFrameDataRegister(SpirvBindingProfile::Scene),
			SceneShaderInputLayout::vertexConstantRegister);
		EXPECT_EQ(SpirvBindingLayout::ResolveParameterDataRegister(SpirvBindingProfile::Scene),
			SceneShaderInputLayout::pixelConstantRegister);
		EXPECT_EQ(SpirvBindingLayout::ResolveFrameDataRegister(SpirvBindingProfile::PostProcess),
			PostProcessInputLayout::frameDataRegister);
		EXPECT_EQ(SpirvBindingLayout::ResolveParameterDataRegister(SpirvBindingProfile::PostProcess),
			PostProcessInputLayout::parameterDataRegister);
		EXPECT_EQ(SpirvBindingLayout::ResolveTextureBinding(0), 0);
		EXPECT_EQ(SpirvBindingLayout::ResolveTextureBinding(3), 3);
		EXPECT_EQ(SpirvBindingLayout::ResolveTextureBinding(4), 7);
		EXPECT_EQ(SpirvBindingLayout::ResolveTextureBinding(7), 10);
		EXPECT_EQ(SpirvBindingLayout::ResolveSamplerBinding(0), 4);
		EXPECT_EQ(SpirvBindingLayout::ResolveSamplerBinding(2), 6);
		EXPECT_EQ(PostProcessInputLayout::samplerCount, 1);
	}

	TEST(GraphicsContract, ResourceOwnersCannotBeCopied) {
		EXPECT_FALSE((std::is_copy_constructible_v<Dx12CommandContext>));
		EXPECT_FALSE((std::is_copy_assignable_v<Dx12CommandContext>));
		EXPECT_FALSE((std::is_copy_constructible_v<OpenGlTextureCache>));
		EXPECT_FALSE((std::is_copy_assignable_v<OpenGlTextureCache>));
		EXPECT_FALSE((std::is_copy_constructible_v<Dx11TextureCache>));
		EXPECT_FALSE((std::is_copy_assignable_v<Dx11TextureCache>));
		EXPECT_FALSE((std::is_copy_constructible_v<Dx12TextureCache>));
		EXPECT_FALSE((std::is_copy_assignable_v<Dx12TextureCache>));
		EXPECT_FALSE((std::is_copy_constructible_v<VulkanTextureCache>));
		EXPECT_FALSE((std::is_copy_assignable_v<VulkanTextureCache>));
	}

	TEST(DrawerContract, RespectsBuiltInPassToggles) {
		DrawerContractTestAdapter drawer;
		SceneDrawState state;
		state.scene.modelEnabled = false;
		state.scene.edgeEnabled = true;
		state.scene.groundShadowEnabled = false;
		drawer.BeginDraw(state);
		ASSERT_TRUE(drawer.DrawModelPass().has_value());
		ASSERT_TRUE(drawer.DrawEdgePass().has_value());
		ASSERT_TRUE(drawer.DrawGroundShadowPass().has_value());
		ASSERT_TRUE(drawer.DrawPostProcessSceneInputs().has_value());
		EXPECT_EQ(drawer.GetBeginCallCount(), 1);
		EXPECT_EQ(drawer.GetModelCallCount(), 0);
		EXPECT_EQ(drawer.GetEdgeCallCount(), 1);
		EXPECT_EQ(drawer.GetGroundShadowCallCount(), 0);
		EXPECT_EQ(drawer.GetSceneInputCallCount(), 1);
	}

	TEST(DrawerContract, ResolvesMaterialVisibilityAndTextureModes) {
		Material material;
		material.edgeFlag = 1;
		material.groundShadow = true;
		material.spTextureMode = SphereMode::Add;
		EXPECT_TRUE(DrawerContractTestAdapter::IsModelMaterialVisible(material));
		EXPECT_TRUE(DrawerContractTestAdapter::IsEdgeMaterialVisible(material));
		EXPECT_TRUE(DrawerContractTestAdapter::IsGroundShadowMaterialVisible(material));
		const glm::ivec3 modes = DrawerContractTestAdapter::ResolveTextureModes(
			material, true, true, true, true);
		EXPECT_EQ(modes.x, 2);
		EXPECT_EQ(modes.y, 1);
		EXPECT_EQ(modes.z, 2);
		material.diffuse.a = 0.0f;
		EXPECT_FALSE(DrawerContractTestAdapter::IsModelMaterialVisible(material));
		EXPECT_FALSE(DrawerContractTestAdapter::IsEdgeMaterialVisible(material));
		EXPECT_FALSE(DrawerContractTestAdapter::IsGroundShadowMaterialVisible(material));
	}

	TEST(ViewerGeometryContract, WritesTemporalVerticesAndConvertsByteIndices) {
		ModelGeometryData geometry;
		geometry.positions = { glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(4.0f, 5.0f, 6.0f) };
		geometry.normals = { glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) };
		geometry.uvs = { glm::vec2(0.25f, 0.5f), glm::vec2(0.75f, 1.0f) };
		geometry.updatePositions = { glm::vec3(7.0f, 8.0f, 9.0f), glm::vec3(10.0f, 11.0f, 12.0f) };
		geometry.previousPositions = { glm::vec3(-1.0f), glm::vec3(-2.0f) };
		std::vector<ViewerVertex> vertices(geometry.positions.size());
		ASSERT_TRUE(ViewerGeometry::WriteVertices(geometry, true, vertices));
		EXPECT_FLOAT_EQ(vertices[0].position.x, 7.0f);
		EXPECT_FLOAT_EQ(vertices[1].position.z, 12.0f);
		EXPECT_FLOAT_EQ(vertices[0].normal.y, 1.0f);
		EXPECT_FLOAT_EQ(vertices[1].uv.x, 0.75f);
		EXPECT_FLOAT_EQ(vertices[0].previousPosition.x, -1.0f);

		geometry.indices = { 0, static_cast<char>(0xff) };
		geometry.indexCount = 2;
		geometry.indexElementSize = 1;
		ViewerIndexData indexData;
		ASSERT_TRUE(ViewerGeometry::BuildIndexData(geometry, indexData));
		EXPECT_EQ(indexData.indexCount, 2);
		EXPECT_EQ(indexData.elementSize, sizeof(uint16_t));
		ASSERT_EQ(indexData.bytes.size(), sizeof(uint16_t) * 2);
		uint16_t convertedIndices[2]{};
		std::memcpy(convertedIndices, indexData.bytes.data(), indexData.bytes.size());
		EXPECT_EQ(convertedIndices[0], 0);
		EXPECT_EQ(convertedIndices[1], 255);
	}
}
