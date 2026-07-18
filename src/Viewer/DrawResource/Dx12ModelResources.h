#pragma once

#include "Viewer/Buffer/Dx12Buffer.h"
#include "Viewer/Synchronization/FrameBuffering.h"
#include "Viewer/Texture/Dx12TextureCache.h"

#include <d3d12.h>
#include <wrl/client.h>
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
		// 한 프레임 upload buffer 안에서 패스별 상수 데이터가 배치되는 offset을 보관한다.
		struct ConstantBufferLayout {
			size_t modelVertex = 0;
			size_t sceneInputVertex = 0;
			size_t groundShadowVertex = 0;
			size_t groundShadowPixel = 0;
			size_t materialBase = 0;
			size_t materialStride = 0;
			size_t modelPixel = 0;
			size_t sceneSurfacePixel = 0;
			size_t edgeVertex = 0;
			size_t edgePixel = 0;
			size_t totalByteSize = 0;
		};

		Dx12Buffer vertexBuffers[FrameBuffering::dx12BufferCount];
		Dx12Buffer indexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[FrameBuffering::dx12BufferCount]{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		UINT indexCount = 0;
		Dx12Buffer constantBuffers[FrameBuffering::dx12BufferCount];
		ConstantBufferLayout constantBufferLayout;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> textureDescriptorHeap;
		std::vector<Dx12ModelMaterial> materials;
	};
}
