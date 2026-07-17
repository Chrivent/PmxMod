#pragma once

#include "Viewer/Drawer/Drawer.h"

#include <d3d11.h>
#include <glm/glm.hpp>

namespace Chrivent {
	class Dx11Instance;
	class Dx11DrawContext;
	struct Dx11ModelResources;
	struct Dx11Texture;

	// D3D11 명령으로 모델의 각 렌더링 패스를 기록한다.
	class Dx11Drawer : public Drawer {
		const Dx11Instance& instance;
		Dx11ModelResources& resources;
		const Dx11DrawContext& drawContext;

		// 텍스처 유무에 따라 실제 SRV 또는 더미 SRV를 픽셀 셰이더 슬롯에 바인딩한다.
		void BindTexture(
			UINT slot, const Dx11Texture& texture, ID3D11SamplerState* sampler,
			ID3D11ShaderResourceView*& lastView, ID3D11SamplerState*& lastSampler) const;

	protected:
		// DirectX depth range로 맞추는 clip 보정 행렬을 반환한다.
		const glm::mat4& ClipMatrix() const override;
		// 일반 메시 패스를 DX11로 렌더링한다.
		bool DrawModel() override;
		// 엣지 패스를 DX11로 렌더링한다.
		bool DrawEdge() override;
		// 지면 그림자 패스를 DX11로 렌더링한다.
		bool DrawGroundShadow() override;
		// 포스트 프로세스용 단일 샘플 depth에 DX11 모델 geometry를 기록한다.
		bool DrawSceneInputs() override;

	public:
		Dx11Drawer(const Dx11Instance& sourceInstance, Dx11ModelResources& sourceResources,
			const Dx11DrawContext& sourceDrawContext, Viewer& sourceViewer);
	};
}
