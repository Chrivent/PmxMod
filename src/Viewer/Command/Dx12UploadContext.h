#pragma once

#include "Viewer/Device/Dx12Device.h"

#include <Windows.h>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	// DX12 정적 GPU 리소스 복사에 사용하는 command list와 동기화 객체를 재사용한다.
	class Dx12UploadContext {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7> enhancedCommandList;
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> retainedResources;
		HANDLE fenceEvent = nullptr;
		UINT64 fenceValue = 0;
		bool batchRecording = false;

		// 부분 생성된 업로드 명령 객체와 동기화 핸들을 초기 상태로 되돌린다.
		void Reset();
		// 현재 디바이스에서 재사용할 업로드 명령 객체를 처음 생성하거나 초기화한다.
		GraphicsResult<void> Begin(const Dx12Device& sourceDevice);
		// 제출한 업로드 명령이 끝날 때까지 전용 fence로 기다린다.
		GraphicsResult<void> SubmitAndWait(const Dx12Device& sourceDevice);

	public:
		Dx12UploadContext() = default;
		~Dx12UploadContext();

		Dx12UploadContext(const Dx12UploadContext&) = delete;
		Dx12UploadContext& operator=(const Dx12UploadContext&) = delete;

		// 여러 정적 리소스 복사를 한 command list에 기록할 batch를 시작한다.
		GraphicsResult<void> BeginBatch(const Dx12Device& sourceDevice);
		// 현재 batch에 texture 복사와 셰이더 읽기 상태 전환을 기록한다.
		GraphicsResult<void> RecordTextureUpload(ID3D12Resource* destination,
			ID3D12Resource* source, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout);
		// 현재 batch를 한 번 제출하고 GPU 완료까지 기다린다.
		GraphicsResult<void> SubmitBatch(const Dx12Device& sourceDevice);
		// 제출하지 않은 현재 batch와 staging resource를 폐기한다.
		void CancelBatch();
		// upload buffer를 정적 GPU buffer에 복사하고 index 입력 상태로 전환한다.
		GraphicsResult<void> UploadIndexBuffer(const Dx12Device& sourceDevice, ID3D12Resource* destination,
			ID3D12Resource* source, UINT64 size);
	};
}
