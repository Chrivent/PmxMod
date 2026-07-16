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
		const Dx11Device& device;
		const Dx11Pipeline& pipeline;
		const Dx11DummyTexture& dummyTexture;

	public:
		Dx11DrawContext(const Dx11Device& sourceDevice, const Dx11Pipeline& sourcePipeline,
			const Dx11DummyTexture& sourceDummyTexture)
			: device(sourceDevice), pipeline(sourcePipeline),
			dummyTexture(sourceDummyTexture) {}

		ID3D11Device* GetDevice() const { return device.GetDevice(); }
		ID3D11DeviceContext* GetDeviceContext() const { return device.GetContext(); }
		const Dx11ShaderSet& GetShaders() const { return pipeline.GetShaders(); }
		const Dx11PipelineStates& GetPipelineStates() const { return pipeline.GetStates(); }
		const Dx11DummyTexture& GetDummyTexture() const { return dummyTexture; }

		// 현재 출력 크기에 맞는 viewport를 immediate context에 적용한다.
		static void ApplyViewport(ID3D11DeviceContext* context, int width, int height);
	};
}
