#pragma once

#include "Viewer/Device/Dx11Device.h"
#include "Viewer/Pipeline/Dx11Pipeline.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	// texture가 없는 D3D11 재질에 바인딩할 기본 흰색 texture를 보관한다.
	struct Dx11DummyTexture {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
	};

	// D3D11 Drawer와 Instance에 장면 그리기에 필요한 API 리소스만 노출한다.
	class Dx11DrawContext {
		enum class BoundPipeline {
			None,
			Model,
			Edge,
			GroundShadow,
			DepthOnly,
			SceneVelocity
		};

		const Dx11Device& device;
		const Dx11Pipeline& pipeline;
		const Dx11DummyTexture& dummyTexture;
		BoundPipeline boundPipeline = BoundPipeline::None;

	public:
		Dx11DrawContext(const Dx11Device& sourceDevice, const Dx11Pipeline& sourcePipeline,
			const Dx11DummyTexture& sourceDummyTexture)
			: device(sourceDevice), pipeline(sourcePipeline),
			dummyTexture(sourceDummyTexture) {}

		ID3D11DeviceContext* GetDeviceContext() const { return device.GetContext(); }
		ID3D11ShaderResourceView* GetDummyTextureView() const { return dummyTexture.textureView.Get(); }
		ID3D11SamplerState* GetTextureSampler() const { return pipeline.GetTextureSampler(); }
		ID3D11SamplerState* GetToonTextureSampler() const { return pipeline.GetToonTextureSampler(); }

		// 새 프레임에서 외부 패스가 바꿀 수 있는 pipeline 바인딩 캐시를 초기화한다.
		void BeginFrame() { boundPipeline = BoundPipeline::None; }
		// 재질의 양면 여부에 맞는 rasterizer state를 반환한다.
		ID3D11RasterizerState* ResolveModelRasterizerState(bool bothFace) const;
		// 모델 표면 셰이더와 고정 pipeline state를 바인딩한다.
		void BindModelPipeline();
		// 엣지 셰이더와 고정 pipeline state를 바인딩한다.
		void BindEdgePipeline();
		// 지면 그림자 셰이더와 고정 pipeline state를 바인딩한다.
		void BindGroundShadowPipeline();
		// depth 또는 velocity 장면 입력 셰이더를 바인딩한다.
		void BindSceneInputPipeline(bool velocity);
		// 현재 출력 크기에 맞는 viewport를 immediate context에 적용한다.
		static void ApplyViewport(ID3D11DeviceContext* context, int width, int height);
	};
}
