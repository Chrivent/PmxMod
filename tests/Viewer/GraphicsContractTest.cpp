#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/PostProcess/PostProcessFrameData.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"
#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

#include <gtest/gtest.h>

namespace Chrivent {
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
}
