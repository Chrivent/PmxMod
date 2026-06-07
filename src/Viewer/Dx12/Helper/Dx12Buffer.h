#pragma once

#include "Dx12Device.h"

#include <cstddef>
#include <d3d12.h>
#include <wrl/client.h>

namespace Chrivent {
	class Dx12Buffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		size_t byteSize = 0;

	public:
		ID3D12Resource* GetResource() const { return resource.Get(); }
		D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const;
		size_t GetByteSize() const { return byteSize; }

		// DX12 constant buffer 규칙에 맞게 256바이트 단위로 정렬한다.
		static size_t AlignConstantBufferSize(size_t size);
		// CPU에서 직접 갱신할 수 있는 upload buffer를 생성한다.
		bool InitializeUpload(const Dx12DeviceInfo& deviceInfo, size_t size);
		// upload buffer에 데이터를 복사한다.
		bool Write(const void* data, size_t size) const;
		// 생성한 DX12 buffer 리소스를 해제한다.
		void Destroy();
	};
}
