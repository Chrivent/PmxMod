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
		Dx11DrawContext& drawContext;

		// 텍스처 유무에 따라 실제 SRV 또는 더미 SRV를 픽셀 셰이더 슬롯에 바인딩한다.
		void BindTexture(UINT textureSlot, UINT samplerSlot,
			const Dx11Texture& texture, ID3D11SamplerState* sampler,
			ID3D11ShaderResourceView*& lastView, ID3D11SamplerState*& lastSampler) const;
		// 동적 상수 버퍼를 새 저장소로 매핑하고 한 패스의 상수 데이터를 기록한다.
		GraphicsResult<void> WriteConstantBuffer(ID3D11Buffer* buffer,
			const void* data, size_t size, const char* operation) const;

	protected:
		// DirectX depth range로 맞추는 clip 보정 행렬을 반환한다.
		const glm::mat4& ClipMatrix() const override;
		// 일반 메시 패스를 DX11로 렌더링한다.
		GraphicsResult<void> DrawModel() override;
		// 엣지 패스를 DX11로 렌더링한다.
		GraphicsResult<void> DrawEdge() override;
		// 지면 그림자 패스를 DX11로 렌더링한다.
		GraphicsResult<void> DrawGroundShadow() override;
		// 포스트 프로세스용 단일 샘플 depth에 DX11 모델 geometry를 기록한다.
		GraphicsResult<void> DrawSceneInputs() override;

	public:
		Dx11Drawer(const Dx11Instance& sourceInstance, Dx11ModelResources& sourceResources,
			Dx11DrawContext& sourceDrawContext);
	};
}
