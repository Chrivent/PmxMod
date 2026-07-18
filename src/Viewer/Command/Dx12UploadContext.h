#pragma once

#include "Viewer/Device/Dx12Device.h"

#include <Windows.h>
#include <wrl/client.h>

namespace Chrivent {
	// DX12 정적 GPU 리소스 복사에 사용하는 command list와 동기화 객체를 재사용한다.
	class Dx12UploadContext {
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
		Dx12UploadContext() = default;
		~Dx12UploadContext();

		Dx12UploadContext(const Dx12UploadContext&) = delete;
		Dx12UploadContext& operator=(const Dx12UploadContext&) = delete;

		// upload buffer의 배치 정보를 texture에 복사하고 셰이더 읽기 상태로 전환한다.
		bool UploadTexture(const Dx12Device& sourceDevice, ID3D12Resource* destination,
			ID3D12Resource* source, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout);
		// upload buffer를 정적 GPU buffer에 복사하고 index 입력 상태로 전환한다.
		bool UploadIndexBuffer(const Dx12Device& sourceDevice, ID3D12Resource* destination,
			ID3D12Resource* source, UINT64 size);
	};
}
