#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Viewer/Pipeline/Dx12PipelineBuilder.h"

#include <gtest/gtest.h>

namespace Chrivent {
	TEST(ApiDescriptorContract, Dx11UsesFlipModelAndPreservesGroundShadowAlpha) {
		const DXGI_SWAP_CHAIN_DESC1 swapChainDesc = Dx11DescBuilder::MakeSwapChainDesc();
		EXPECT_EQ(swapChainDesc.BufferCount, 2);
		EXPECT_EQ(swapChainDesc.SampleDesc.Count, 1);
		EXPECT_EQ(swapChainDesc.SwapEffect, DXGI_SWAP_EFFECT_FLIP_DISCARD);
		const D3D11_BLEND_DESC blendDesc = Dx11DescBuilder::MakeGroundShadowBlendDesc();
		EXPECT_EQ(blendDesc.RenderTarget[0].SrcBlendAlpha, D3D11_BLEND_ZERO);
		EXPECT_EQ(blendDesc.RenderTarget[0].DestBlendAlpha, D3D11_BLEND_ONE);
		const D3D11_RASTERIZER_DESC rasterizerDesc =
			Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_BACK, true);
		EXPECT_TRUE(rasterizerDesc.MultisampleEnable);
		EXPECT_TRUE(rasterizerDesc.FrontCounterClockwise);
	}

	TEST(ApiDescriptorContract, Dx12RasterizerUsesSharedSceneDefaults) {
		D3D12_RASTERIZER_DESC rasterizerDesc{};
		Dx12PipelineBuilder::ConfigureRasterizer(rasterizerDesc, D3D12_CULL_MODE_FRONT);
		EXPECT_EQ(rasterizerDesc.FillMode, D3D12_FILL_MODE_SOLID);
		EXPECT_EQ(rasterizerDesc.CullMode, D3D12_CULL_MODE_FRONT);
		EXPECT_TRUE(rasterizerDesc.FrontCounterClockwise);
		EXPECT_TRUE(rasterizerDesc.DepthClipEnable);
	}
}
