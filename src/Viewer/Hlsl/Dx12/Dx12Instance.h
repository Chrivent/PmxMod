#pragma once

#include "../../Instance.h"
#include "../../ViewerGeometry.h"
#include "Helper/Dx12Buffer.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <vector>

namespace Chrivent {
	class Dx12Drawer;
	class Dx12Viewer;
	struct Dx12Material;

	using Dx12Vertex = ViewerVertex;

	struct Dx12InstanceInfo : InstanceInfo {
		Dx12Viewer* viewer = nullptr;
		Dx12Buffer vertexBuffer;
		Dx12Buffer indexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		UINT indexCount = 0;
		Dx12Buffer modelVertexConstantBuffer;
		std::vector<Dx12Buffer> modelPixelConstantBuffers;
		Dx12Buffer edgeVertexConstantBuffer;
		std::vector<Dx12Buffer> edgeSizeConstantBuffers;
		std::vector<Dx12Buffer> edgePixelConstantBuffers;
		Dx12Buffer groundShadowVertexConstantBuffer;
		Dx12Buffer groundShadowPixelConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> textureDescriptorHeap;
		UINT textureDescriptorSize = 0;
		std::vector<Dx12Material> materials;
	};

	class Dx12Instance : public Instance {
		// material별 텍스처 SRV descriptor를 생성한다.
		static bool CreateTextureDescriptors(Dx12InstanceInfo& info);

	public:
		Dx12Instance();
		~Dx12Instance() override = default;

		// DX12 모델 리소스를 해제한다.
		void Clear() override;
		// 모델 데이터를 DX12 리소스로 업로드한다.
		bool Setup(Viewer& baseViewer) override;
		// 모델의 갱신된 버텍스 데이터를 DX12 리소스에 반영한다.
		void Update() const override;
	};
}
