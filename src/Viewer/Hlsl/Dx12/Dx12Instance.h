#pragma once

#include "../../Instance.h"
#include "Helper/Dx12Buffer.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <array>
#include <vector>

namespace Chrivent {
	class Dx12Drawer;
	class Dx12Viewer;
	struct Dx12Material;

	struct Dx12InstanceInfo : InstanceInfo {
		static constexpr size_t kBufferedFrames = 2;
		Dx12Viewer* viewer = nullptr;
		std::array<Dx12Buffer, kBufferedFrames> vertexBuffers;
		Dx12Buffer indexBuffer;
		std::array<D3D12_VERTEX_BUFFER_VIEW, kBufferedFrames> vertexBufferViews{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		UINT indexCount = 0;
		std::array<Dx12Buffer, kBufferedFrames> modelVertexConstantBuffers;
		std::vector<std::array<Dx12Buffer, kBufferedFrames>> modelPixelConstantBuffers;
		std::array<Dx12Buffer, kBufferedFrames> edgeVertexConstantBuffers;
		std::vector<std::array<Dx12Buffer, kBufferedFrames>> edgeSizeConstantBuffers;
		std::vector<std::array<Dx12Buffer, kBufferedFrames>> edgePixelConstantBuffers;
		std::array<Dx12Buffer, kBufferedFrames> groundShadowVertexConstantBuffers;
		std::array<Dx12Buffer, kBufferedFrames> groundShadowPixelConstantBuffers;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> textureDescriptorHeap;
		UINT textureDescriptorSize = 0;
		std::vector<Dx12Material> materials;
	};

	class Dx12Instance : public Instance {
		// 모델 geometry 데이터를 DX12 vertex/index buffer로 업로드한다.
		static bool CreateGeometryBuffers(Dx12InstanceInfo& info, const Dx12DeviceInfo& deviceInfo);
		// 패스별 constant buffer를 material 개수에 맞춰 생성한다.
		static bool CreateConstantBuffers(Dx12InstanceInfo& info, const Dx12DeviceInfo& deviceInfo);
		// 모델 material 정보를 DX12 material 캐시와 texture descriptor 준비 데이터로 변환한다.
		static void LoadMaterials(Dx12InstanceInfo& info);
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
		void Upload() const override;
	};
}
