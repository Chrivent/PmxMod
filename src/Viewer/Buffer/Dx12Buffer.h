#pragma once

#include "Viewer/Device/Dx12Device.h"

#include <cstddef>
#include <d3d12.h>
#include <span>
#include <type_traits>
#include <wrl/client.h>

namespace Chrivent {
	// DX12 버퍼 리소스와 매핑된 CPU 주소를 소유한다.
	class Dx12Buffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		void* mappedData = nullptr;
		size_t byteSize = 0;

	public:
		Dx12Buffer() = default;
		~Dx12Buffer() { Reset(); }

		Dx12Buffer(const Dx12Buffer&) = delete;
		Dx12Buffer& operator=(const Dx12Buffer&) = delete;

		bool IsInitialized() const { return resource != nullptr; }
		void* GetMappedData() const { return mappedData; }
		D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const {
			return resource ? resource->GetGPUVirtualAddress() : 0;
		}

		// DX12 constant buffer 규칙에 맞게 256바이트 단위로 정렬한다.
		static size_t AlignConstantBufferSize(size_t size);
		// CPU에서 직접 갱신할 수 있는 upload buffer를 생성한다.
		bool InitializeUpload(const Dx12Device& sourceDevice, size_t size);
		// upload buffer의 지정한 byte offset에 데이터를 복사한다.
		bool Write(std::span<const std::byte> data, size_t offset) const;
		// upload buffer에 데이터를 복사한다.
		bool Write(std::span<const std::byte> data) const { return Write(data, 0); }
		// trivially copyable 값 하나를 upload buffer의 지정한 byte offset에 복사한다.
		template <typename T> requires std::is_trivially_copyable_v<T>
		bool Write(const T& data, const size_t offset) const {
			return Write(std::as_bytes(std::span{ &data, 1 }), offset);
		}
		// trivially copyable 값 하나를 upload buffer에 복사한다.
		template <typename T> requires std::is_trivially_copyable_v<T>
		bool Write(const T& data) const { return Write(std::as_bytes(std::span{ &data, 1 })); }
		// 생성한 DX12 buffer 리소스를 해제한다.
		void Reset();
		// 두 버퍼의 소유 리소스와 매핑 상태를 교환한다.
		void Swap(Dx12Buffer& other) noexcept;
	};
}
