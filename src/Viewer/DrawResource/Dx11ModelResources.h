#pragma once

#include "Viewer/Texture/Dx11TextureCache.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <vector>

namespace Chrivent {
	struct Material;

	// 공통 PMX 재질에 D3D11 텍스처 리소스를 결합한다.
	struct Dx11ModelMaterial {
		const Material& material;
		Dx11Texture texture{};
		Dx11Texture sphereTexture{};
		Dx11Texture toonTexture{};

		explicit Dx11ModelMaterial(const Material& sourceMaterial) : material(sourceMaterial) {}
	};

	// DX11이 한 모델을 그릴 때 사용하는 GPU 리소스를 보관한다.
	struct Dx11ModelResources {
		std::vector<Dx11ModelMaterial> materials;
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
		DXGI_FORMAT indexBufferFormat = DXGI_FORMAT_R16_UINT;
		Microsoft::WRL::ComPtr<ID3D11Buffer> vsConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> psConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> sceneSurfaceConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> edgeVsConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> edgePsConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> gsVsConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> gsPsConstantBuffer;
	};
}
