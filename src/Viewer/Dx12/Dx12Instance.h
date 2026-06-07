#pragma once

#include "../Instance.h"
#include "Helper/Dx12Buffer.h"

#include <d3d12.h>
#include <glm/glm.hpp>
#include <wrl/client.h>

#include <vector>

namespace Chrivent {
	class Dx12Drawer;
	class Dx12Viewer;
	struct Dx12Material;
	struct ModelGeometryData;

	struct Dx12Vertex {
		glm::vec3 position{};
		glm::vec3 normal{};
		glm::vec2 uv{};
	};

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
		// 모델 geometry를 DX12 vertex 배열로 변환한다.
		static std::vector<Dx12Vertex> BuildVertices(const ModelGeometryData& geometryData, bool useUpdateData);
		// PMX index element 크기에 맞춰 DX12 index buffer 데이터를 만든다.
		static bool BuildIndexData(const ModelGeometryData& geometryData, std::vector<char>& indices, DXGI_FORMAT& format);
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
