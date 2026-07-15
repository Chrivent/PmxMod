#pragma once

#include "Viewer/Instance/Instance.h"
#include "Viewer/Buffer/Dx12Buffer.h"
#include "Viewer/Texture/Dx12TextureCache.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <vector>

namespace Chrivent {
	class Dx12Drawer;
	class Dx12Viewer;

	// 공통 PMX 재질에 D3D12 texture와 descriptor handle을 결합한다.
	struct Dx12Material : ViewerMaterial {
		Dx12Texture texture{};
		Dx12Texture sphereTexture{};
		Dx12Texture toonTexture{};
		D3D12_GPU_DESCRIPTOR_HANDLE textureDescriptorHandle{};

		explicit Dx12Material(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
	};

	// 한 모델의 D3D12 버퍼, 재질과 descriptor 상태를 관리한다.
	class Dx12Instance : public Instance {
		Dx12Viewer& viewer;

		// 모델 geometry 데이터를 DX12 vertex/index buffer로 업로드한다.
		bool CreateGeometryBuffers(const Dx12Device& device);
		// 패스별 constant buffer를 material 개수에 맞춰 생성한다.
		bool CreateConstantBuffers(const Dx12Device& device);
		// 모델 material 정보를 DX12 material 캐시와 texture descriptor 준비 데이터로 변환한다.
		void LoadMaterials();
		// material별 텍스처 SRV descriptor를 생성한다.
		bool CreateTextureDescriptors();
		
	protected:
		// DX12 모델 GPU 리소스를 초기 상태로 되돌린다.
		void ResetRendererResources() override;
		// DX12 모델 리소스를 생성하고 인스턴스를 초기화한다.
		bool SetupRenderer() override;

	public:
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
		std::vector<Dx12Material> materials;

		explicit Dx12Instance(Dx12Viewer& sourceViewer);

		// 모델의 갱신된 버텍스 데이터를 DX12 리소스에 반영한다.
		bool Upload() const override;
	};
}
