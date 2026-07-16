#pragma once

#include "Viewer/Device/Dx12Device.h"

#include <Windows.h>
#include <wrl/client.h>

namespace Chrivent {
	// DX12 텍스처 복사에 사용하는 command list와 동기화 객체를 재사용한다.
	class Dx12TextureUploadContext {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7> enhancedCommandList;
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;
		HANDLE fenceEvent = nullptr;
		UINT64 fenceValue = 0;

		// 현재 디바이스에서 재사용할 업로드 명령 객체를 처음 생성하거나 초기화한다.
		bool Begin(const Dx12Device& sourceDevice);
		// 제출한 업로드 명령이 끝날 때까지 전용 fence로 기다린다.
		bool SubmitAndWait(const Dx12Device& sourceDevice);

	public:
		Dx12TextureUploadContext() = default;
		Dx12TextureUploadContext(const Dx12TextureUploadContext&) = delete;
		Dx12TextureUploadContext& operator=(const Dx12TextureUploadContext&) = delete;
		~Dx12TextureUploadContext();

		// upload buffer의 배치 정보를 texture에 복사하고 셰이더 읽기 상태로 전환한다.
		bool Upload(const Dx12Device& sourceDevice, ID3D12Resource* destination,
			ID3D12Resource* source, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout);
	};
}
