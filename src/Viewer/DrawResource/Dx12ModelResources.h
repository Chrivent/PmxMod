#pragma once

#include "Viewer/Buffer/Dx12Buffer.h"
#include "Viewer/Texture/Dx12TextureCache.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace Chrivent {
	struct Material;

	// 공통 PMX 재질에 D3D12 텍스처와 descriptor handle을 결합한다.
	struct Dx12ModelMaterial {
		const Material& material;
		Dx12Texture texture{};
		Dx12Texture sphereTexture{};
		Dx12Texture toonTexture{};
		D3D12_GPU_DESCRIPTOR_HANDLE textureDescriptorHandle{};

		explicit Dx12ModelMaterial(const Material& sourceMaterial) : material(sourceMaterial) {}
	};

	// DX12가 한 모델을 그릴 때 사용하는 GPU 리소스를 보관한다.
	struct Dx12ModelResources {
		static constexpr size_t kBufferedFrames = 2;

		Dx12Buffer vertexBuffers[kBufferedFrames];
		Dx12Buffer indexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[kBufferedFrames]{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		UINT indexCount = 0;
		Dx12Buffer modelVertexConstantBuffers[kBufferedFrames];
		std::vector<std::unique_ptr<Dx12Buffer[]>> modelPixelConstantBuffers;
		std::vector<std::unique_ptr<Dx12Buffer[]>> edgeVertexConstantBuffers;
		std::vector<std::unique_ptr<Dx12Buffer[]>> edgePixelConstantBuffers;
		Dx12Buffer groundShadowVertexConstantBuffers[kBufferedFrames];
		Dx12Buffer groundShadowPixelConstantBuffers[kBufferedFrames];
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> textureDescriptorHeap;
		UINT textureDescriptorSize = 0;
		std::vector<Dx12ModelMaterial> materials;
	};
}
