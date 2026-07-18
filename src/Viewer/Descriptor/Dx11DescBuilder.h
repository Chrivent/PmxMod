#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>

namespace Chrivent {
	// 반복해서 사용하는 D3D11 리소스 설명 구조체를 생성한다.
	class Dx11DescBuilder {
	public:
		// 지정한 필터와 주소 모드로 샘플러 설명자를 만든다.
		static D3D11_SAMPLER_DESC MakeSamplerDesc(D3D11_FILTER f, D3D11_TEXTURE_ADDRESS_MODE addr);
		// 컬링 모드와 front-face 방향으로 래스터라이저 설명자를 만든다.
		static D3D11_RASTERIZER_DESC MakeRasterizerDesc(D3D11_CULL_MODE cull, bool frontCcw);
		// 일반 알파 블렌딩용 블렌드 설명자를 만든다.
		static D3D11_BLEND_DESC MakeAlphaBlendDesc();
		// 지면 그림자가 기존 render target alpha를 보존하는 블렌드 설명자를 만든다.
		static D3D11_BLEND_DESC MakeGroundShadowBlendDesc();
		// 단일 샘플 flip-model back buffer를 사용하는 스왑체인 설명자를 만든다.
		static DXGI_SWAP_CHAIN_DESC1 MakeSwapChainDesc();
		// 2D 텍스처의 기본 설명자를 만든다.
		static D3D11_TEXTURE2D_DESC MakeTexture2DDesc(UINT width, UINT height, DXGI_FORMAT format,
			UINT bindFlags, UINT sampleCount = 1, UINT sampleQuality = 0);
		// 매 프레임 갱신할 동적 버텍스 버퍼 설명자를 만든다.
		static D3D11_BUFFER_DESC MakeDynamicVertexBufferDesc(UINT byteWidth);
		// 한 번 업로드할 immutable 인덱스 버퍼 설명자를 만든다.
		static D3D11_BUFFER_DESC MakeImmutableIndexBufferDesc(UINT byteWidth);
		// 기본 깊이 테스트용 depth-stencil 설명자를 만든다.
		static D3D11_DEPTH_STENCIL_DESC MakeDefaultDepthStencilDesc();
		// 지면 그림자 스텐실 처리용 depth-stencil 설명자를 만든다.
		static D3D11_DEPTH_STENCIL_DESC MakeGroundShadowDepthStencilDesc();
	};
}
