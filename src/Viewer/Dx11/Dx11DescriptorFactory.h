#pragma once

#include <d3d11.h>

namespace Chrivent {
	class Dx11DescriptorFactory {
	public:
		// 지정한 필터와 주소 모드로 샘플러 설명자를 만든다.
		static D3D11_SAMPLER_DESC MakeSamplerDesc(D3D11_FILTER f, D3D11_TEXTURE_ADDRESS_MODE addr);
		// 컬링 모드와 front-face 방향으로 래스터라이저 설명자를 만든다.
		static D3D11_RASTERIZER_DESC MakeRasterizerDesc(D3D11_CULL_MODE cull, bool frontCcw);
		// 일반 알파 블렌딩용 블렌드 설명자를 만든다.
		static D3D11_BLEND_DESC MakeAlphaBlendDesc();
		// GLFW 윈도우와 MSAA 설정으로 스왑체인 설명자를 만든다.
		static DXGI_SWAP_CHAIN_DESC MakeSwapChainDesc(HWND__* hwnd, UINT sampleCount, UINT sampleQuality);
		// 2D 텍스처의 기본 설명자를 만든다.
		static D3D11_TEXTURE2D_DESC MakeTexture2DDesc(UINT width, UINT height, DXGI_FORMAT format,
			UINT bindFlags, UINT sampleCount = 1, UINT sampleQuality = 0);
		// 기본 깊이 테스트용 depth-stencil 설명자를 만든다.
		static D3D11_DEPTH_STENCIL_DESC MakeDefaultDepthStencilDesc();
		// 지면 그림자 스텐실 처리용 depth-stencil 설명자를 만든다.
		static D3D11_DEPTH_STENCIL_DESC MakeGroundShadowDepthStencilDesc();
	};
}
