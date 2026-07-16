#include "Viewer/RenderTarget/Dx12MsaaColorBuffer.h"

#include "Viewer/Synchronization/Dx12Barrier.h"

namespace Chrivent {
	bool Dx12MsaaColorBuffer::Initialize(const Dx12Device& sourceDevice, const int width, const int height) {
		Reset();
		if (!sourceDevice.device || width <= 0 || height <= 0)
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		if (FAILED(sourceDevice.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap))))
			return false;
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 1.0f;
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		resourceDesc.SampleDesc.Count = sourceDevice.msaaSampleCount;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		if (FAILED(sourceDevice.device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
			&clearValue, IID_PPV_ARGS(&renderTarget))))
			return false;
		sourceDevice.device->CreateRenderTargetView(renderTarget.Get(), nullptr, GetRtvHandle());
		sampleCount = sourceDevice.msaaSampleCount;
		return true;
	}

	bool Dx12MsaaColorBuffer::ResolveToBackBuffer(ID3D12GraphicsCommandList* commandList,
		ID3D12GraphicsCommandList7* enhancedCommandList, ID3D12Resource* backBuffer) const {
		if (commandList == nullptr || backBuffer == nullptr || !renderTarget)
			return false;
		const D3D12_RESOURCE_STATES sourceState = sampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;
		const D3D12_RESOURCE_STATES destinationState = sampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_DEST : D3D12_RESOURCE_STATE_COPY_DEST;
		Dx12Barrier::Transition(commandList, enhancedCommandList, renderTarget.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, sourceState);
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, destinationState);
		if (sampleCount > 1)
			commandList->ResolveSubresource(backBuffer, 0, renderTarget.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			commandList->CopyResource(backBuffer, renderTarget.Get());
		Dx12Barrier::Transition(commandList, enhancedCommandList, renderTarget.Get(),
			sourceState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			destinationState, D3D12_RESOURCE_STATE_PRESENT);
		return true;
	}

	void Dx12MsaaColorBuffer::Reset() {
		renderTarget.Reset();
		rtvHeap.Reset();
		sampleCount = 1;
	}
}
