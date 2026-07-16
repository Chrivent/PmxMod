#include "Viewer/Descriptor/Dx11DescBuilder.h"

namespace Chrivent {
	D3D11_SAMPLER_DESC Dx11DescBuilder::MakeSamplerDesc(const D3D11_FILTER f, const D3D11_TEXTURE_ADDRESS_MODE addr) {
		CD3D11_SAMPLER_DESC d(D3D11_DEFAULT);
		d.Filter = f;
		d.AddressU = d.AddressV = d.AddressW = addr;
		return d;
	}

	D3D11_RASTERIZER_DESC Dx11DescBuilder::MakeRasterizerDesc(const D3D11_CULL_MODE cull, const bool frontCcw) {
		CD3D11_RASTERIZER_DESC d(D3D11_DEFAULT);
		d.CullMode = cull;
		d.FrontCounterClockwise = frontCcw;
		d.MultisampleEnable = TRUE;
		return d;
	}

	D3D11_BLEND_DESC Dx11DescBuilder::MakeAlphaBlendDesc() {
		CD3D11_BLEND_DESC d(D3D11_DEFAULT);
		d.RenderTarget[0].BlendEnable = TRUE;
		d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		return d;
	}

	D3D11_BLEND_DESC Dx11DescBuilder::MakeGroundShadowBlendDesc() {
		D3D11_BLEND_DESC d = MakeAlphaBlendDesc();
		d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		return d;
	}

	DXGI_SWAP_CHAIN_DESC Dx11DescBuilder::MakeSwapChainDesc(HWND__* hwnd, const UINT sampleCount, const UINT sampleQuality) {
		DXGI_SWAP_CHAIN_DESC d{};
		d.BufferCount = 2;
		d.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		d.OutputWindow = hwnd;
		d.SampleDesc.Count = sampleCount;
		d.SampleDesc.Quality = sampleQuality;
		d.Windowed = TRUE;
		return d;
	}

	D3D11_TEXTURE2D_DESC Dx11DescBuilder::MakeTexture2DDesc(
		const UINT width, const UINT height, const DXGI_FORMAT format,
		const UINT bindFlags, const UINT sampleCount, const UINT sampleQuality) {
		D3D11_TEXTURE2D_DESC d{};
		d.Width = width;
		d.Height = height;
		d.MipLevels = 1;
		d.ArraySize = 1;
		d.Format = format;
		d.SampleDesc.Count = sampleCount;
		d.SampleDesc.Quality = sampleQuality;
		d.Usage = D3D11_USAGE_DEFAULT;
		d.BindFlags = bindFlags;
		return d;
	}

	D3D11_BUFFER_DESC Dx11DescBuilder::MakeDynamicVertexBufferDesc(const UINT byteWidth) {
		D3D11_BUFFER_DESC d{};
		d.Usage = D3D11_USAGE_DYNAMIC;
		d.ByteWidth = byteWidth;
		d.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		return d;
	}

	D3D11_BUFFER_DESC Dx11DescBuilder::MakeImmutableIndexBufferDesc(const UINT byteWidth) {
		D3D11_BUFFER_DESC d{};
		d.Usage = D3D11_USAGE_IMMUTABLE;
		d.ByteWidth = byteWidth;
		d.BindFlags = D3D11_BIND_INDEX_BUFFER;
		return d;
	}

	D3D11_DEPTH_STENCIL_DESC Dx11DescBuilder::MakeDefaultDepthStencilDesc() {
		CD3D11_DEPTH_STENCIL_DESC d(CD3D11_DEFAULT{});
		d.DepthEnable = TRUE;
		d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		d.DepthFunc = D3D11_COMPARISON_LESS;
		d.StencilEnable = FALSE;
		return d;
	}

	D3D11_DEPTH_STENCIL_DESC Dx11DescBuilder::MakeGroundShadowDepthStencilDesc() {
		CD3D11_DEPTH_STENCIL_DESC d(CD3D11_DEFAULT{});
		d.DepthEnable = TRUE;
		d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		d.DepthFunc = D3D11_COMPARISON_LESS;
		d.StencilEnable = TRUE;
		d.StencilReadMask = 0x01;
		d.StencilWriteMask = 0xFF;
		d.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
		d.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		d.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		d.BackFace = d.FrontFace;
		return d;
	}
}
