#pragma once

#include "Dx12Device.h"

#include <cstddef>
#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12Buffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		void* mappedData = nullptr;
		size_t byteSize = 0;

	public:
		// DX12 buffer resource가 생성되어 있는지 확인한다.
		bool IsInitialized() const { return resource != nullptr; }
		void* ResolveMappedData() const { return mappedData; }

		// DX12 constant buffer 규칙에 맞게 256바이트 단위로 정렬한다.
		static size_t AlignConstantBufferSize(size_t size);
		// DX12 resource의 GPU virtual address를 해석해 반환한다.
		D3D12_GPU_VIRTUAL_ADDRESS ResolveGpuAddress() const;
		// CPU에서 직접 갱신할 수 있는 upload buffer를 생성한다.
		bool InitializeUpload(const Dx12DeviceInfo& deviceInfo, size_t size);
		// upload buffer에 데이터를 복사한다.
		bool Write(const void* data, size_t size) const;
		// 생성한 DX12 buffer 리소스를 해제한다.
		void Destroy();
	};
}
