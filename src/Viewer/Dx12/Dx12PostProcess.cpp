#include "Viewer/Dx12/Dx12PostProcess.h"

#include "Viewer/Dx12/Helper/Dx12Barrier.h"
#include "Viewer/Dx12/Helper/Dx12CommandContext.h"
#include "Viewer/Dx12/Helper/Dx12Pipeline.h"
#include "Viewer/Dx12/Helper/Dx12SwapChain.h"
#include "Viewer/Shader/PostProcessInputLayout.h"

namespace Chrivent {
	void Dx12PostProcess::ResolveToBackBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer,
		ID3D12Resource* msaaColor, const Dx12Device& sourceDevice, const Dx12CommandContext& commandContext) {
		const D3D12_RESOURCE_STATES sourceState = sourceDevice.msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_SOURCE
			: D3D12_RESOURCE_STATE_COPY_SOURCE;
		const D3D12_RESOURCE_STATES destinationState = sourceDevice.msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_DEST
			: D3D12_RESOURCE_STATE_COPY_DEST;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			D3D12_RESOURCE_STATE_RENDER_TARGET, sourceState);
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, destinationState);
		if (sourceDevice.msaaSampleCount > 1)
			commandList->ResolveSubresource(backBuffer, 0, msaaColor, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			commandList->CopyResource(backBuffer, msaaColor);
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			sourceState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			destinationState, D3D12_RESOURCE_STATE_PRESENT);
	}

	void Dx12PostProcess::ApplyViewportAndScissor(
		ID3D12GraphicsCommandList* commandList, const int width, const int height) {
		D3D12_VIEWPORT viewport{};
		viewport.Width = width;
		viewport.Height = height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &viewport);
		D3D12_RECT scissorRect{};
		scissorRect.right = width;
		scissorRect.bottom = height;
		commandList->RSSetScissorRects(1, &scissorRect);
	}

	bool Dx12PostProcess::CreateDepthTarget(const Dx12Device& sourceDevice, const int width, const int height) {
		depth.Reset();
		depthDsvHeap.Reset();
		focusHistory[0].Reset();
		focusHistory[1].Reset();
		focusHistoryRtvHeap.Reset();
		focusHistorySrvHeap.Reset();
		focusHistoryInitialized = false;
		focusHistoryIndex = 0;
		if (!sourceDevice.device || width <= 0 || height <= 0)
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		if (FAILED(sourceDevice.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&depthDsvHeap))))
			return false;
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
		resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clearValue.DepthStencil.Depth = 1.0f;
		if (FAILED(sourceDevice.device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&depth))))
			return false;
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		sourceDevice.device->CreateDepthStencilView(depth.Get(), &dsvDesc, ResolveDepthDsvHandle());
		if (!CreateFocusHistoryTargets(sourceDevice))
			return false;
		for (const auto& target : targets) {
			target.UpdateDepthShaderResourceView(sourceDevice, depth.Get());
			target.UpdateFocusHistoryShaderResourceView(sourceDevice, nullptr);
		}
		return true;
	}

	bool Dx12PostProcess::CreateFocusHistoryTargets(const Dx12Device& sourceDevice) {
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = 2;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		if (FAILED(sourceDevice.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&focusHistoryRtvHeap))))
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
		srvHeapDesc.NumDescriptors = PostProcessInputLayout::RequiredTextureCount;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (FAILED(sourceDevice.device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&focusHistorySrvHeap))))
			return false;
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = 1;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		for (int index = 0; index < 2; index++) {
			if (FAILED(sourceDevice.device->CreateCommittedResource(
				&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&focusHistory[index]))))
				return false;
			sourceDevice.device->CreateRenderTargetView(
				focusHistory[index].Get(), nullptr, ResolveFocusHistoryRtvHandle(sourceDevice, index));
		}
		UpdateFocusHistoryShaderResources(sourceDevice, 0);
		return true;
	}

	void Dx12PostProcess::UpdateFocusHistoryShaderResources(const Dx12Device& sourceDevice, const int readIndex) const {
		if (!focusHistorySrvHeap || targets.empty())
			return;
		ID3D12Device* device = sourceDevice.device.Get();
		const UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE handle = focusHistorySrvHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_SHADER_RESOURCE_VIEW_DESC colorSrvDesc{};
		colorSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		colorSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		colorSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		colorSrvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(targets[0].ResolveResource(), &colorSrvDesc, handle);
		handle.ptr += increment;
		D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
		depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		depthSrvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(depth.Get(), &depthSrvDesc, handle);
		handle.ptr += increment;
		D3D12_SHADER_RESOURCE_VIEW_DESC focusSrvDesc{};
		focusSrvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		focusSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		focusSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		focusSrvDesc.Texture2D.MipLevels = 1;
		ID3D12Resource* readResource = focusHistoryInitialized ? focusHistory[readIndex].Get() : nullptr;
		device->CreateShaderResourceView(readResource, &focusSrvDesc, handle);
	}

	void Dx12PostProcess::UpdateFocusHistory(ID3D12GraphicsCommandList* commandList, const Dx12Device& sourceDevice,
		const Dx12CommandContext& commandContext, const Dx12Pipeline& pipeline, const int width, const int height) {
		if (!pipeline.HasFocusHistoryEffect() || !focusHistory[0] || !focusHistory[1] || !focusHistorySrvHeap)
			return;
		const int readIndex = focusHistoryIndex;
		const int writeIndex = 1 - focusHistoryIndex;
		UpdateFocusHistoryShaderResources(sourceDevice, readIndex);
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, focusHistory[writeIndex].Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = ResolveFocusHistoryRtvHandle(sourceDevice, writeIndex);
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
		ApplyViewportAndScissor(commandList, 1, 1);
		ID3D12DescriptorHeap* descriptorHeaps[] = { focusHistorySrvHeap.Get() };
		commandList->SetDescriptorHeaps(1, descriptorHeaps);
		pipeline.BindFocusHistory(commandList, ResolveFocusHistoryGpuHandle());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
		Dx12Barrier::Transition(commandList, enhancedCommandList, focusHistory[writeIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		focusHistoryIndex = writeIndex;
		focusHistoryInitialized = true;
		ApplyViewportAndScissor(commandList, width, height);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12PostProcess::ResolveDepthDsvHandle() const {
		if (!depthDsvHeap)
			return {};
		return depthDsvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12PostProcess::ResolveFocusHistoryRtvHandle(
		const Dx12Device& sourceDevice, const int index) const {
		if (!focusHistoryRtvHeap)
			return {};
		D3D12_CPU_DESCRIPTOR_HANDLE handle = focusHistoryRtvHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += sourceDevice.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * index;
		return handle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Dx12PostProcess::ResolveFocusHistoryGpuHandle() const {
		if (!focusHistorySrvHeap)
			return {};
		return focusHistorySrvHeap->GetGPUDescriptorHandleForHeapStart();
	}

	bool Dx12PostProcess::InitializeTargets(const Dx12Device& sourceDevice, const int width, const int height) {
		targets.resize(targetCount);
		for (auto& target : targets) {
			if (!target.Initialize(sourceDevice, width, height))
				return false;
		}
		return CreateDepthTarget(sourceDevice, width, height);
	}

	bool Dx12PostProcess::Load(
		const Dx12Device& sourceDevice, Dx12Pipeline& pipeline, const std::vector<const EffectDefinition*>& effects) {
		SetEffects(effects);
		focusHistoryInitialized = false;
		focusHistoryIndex = 0;
		if (!pipeline.LoadPostProcessEffects(sourceDevice, ResolveEffectPointers())) {
			ClearEffects();
			return false;
		}
		return true;
	}

	void Dx12PostProcess::ClearPipelines(Dx12Pipeline& pipeline) {
		pipeline.ClearPostProcessEffects();
		ClearEffects();
		focusHistoryInitialized = false;
		focusHistoryIndex = 0;
	}

	bool Dx12PostProcess::BeginDepthPass(
		ID3D12GraphicsCommandList* commandList, const Dx12CommandContext& commandContext, const int width, const int height) const {
		if (!HasEffects() || !depth || commandList == nullptr)
			return false;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, depth.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = ResolveDepthDsvHandle();
		commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		ApplyViewportAndScissor(commandList, width, height);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		return true;
	}

	void Dx12PostProcess::EndDepthPass(
		ID3D12GraphicsCommandList* commandList, const Dx12CommandContext& commandContext) const {
		if (!depth || commandList == nullptr)
			return;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, depth.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void Dx12PostProcess::Draw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer, ID3D12Resource* msaaColor,
		const Dx12Device& sourceDevice, const Dx12CommandContext& commandContext,
		const Dx12SwapChain& swapChain, const Dx12Pipeline& pipeline, const int width, const int height) {
		if (targets.size() < targetCount) {
			ResolveToBackBuffer(commandList, backBuffer, msaaColor, sourceDevice, commandContext);
			return;
		}
		ID3D12Resource* sceneColor = targets[0].ResolveResource();
		const D3D12_RESOURCE_STATES sourceState = sourceDevice.msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_SOURCE
			: D3D12_RESOURCE_STATE_COPY_SOURCE;
		const D3D12_RESOURCE_STATES destinationState = sourceDevice.msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_DEST
			: D3D12_RESOURCE_STATE_COPY_DEST;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			D3D12_RESOURCE_STATE_RENDER_TARGET, sourceState);
		Dx12Barrier::Transition(commandList, enhancedCommandList, sceneColor,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, destinationState);
		if (sourceDevice.msaaSampleCount > 1)
			commandList->ResolveSubresource(sceneColor, 0, msaaColor, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			commandList->CopyResource(sceneColor, msaaColor);
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			sourceState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Dx12Barrier::Transition(commandList, enhancedCommandList, sceneColor,
			destinationState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		UpdateFocusHistory(commandList, sourceDevice, commandContext, pipeline, width, height);
		ID3D12Resource* focusHistoryResource = pipeline.HasFocusHistoryEffect() && focusHistoryInitialized
			? focusHistory[focusHistoryIndex].Get() : nullptr;
		for (const auto& target : targets)
			target.UpdateFocusHistoryShaderResourceView(sourceDevice, focusHistoryResource);
		const size_t passCount = pipeline.GetPostProcessPassCount();
		for (size_t passIndex = 0; passIndex < passCount; passIndex++) {
			const bool lastPass = passIndex + 1 == passCount;
			const size_t sourceIndex = passIndex == 0 ? 0 : (passIndex - 1) % 2 + 1;
			const size_t targetIndex = passIndex % 2 + 1;
			if (!lastPass) {
				ID3D12Resource* target = targets[targetIndex].ResolveResource();
				Dx12Barrier::Transition(commandList, enhancedCommandList, target,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				const D3D12_CPU_DESCRIPTOR_HANDLE targetRtv = targets[targetIndex].ResolveRtvHandle();
				commandList->OMSetRenderTargets(1, &targetRtv, FALSE, nullptr);
			} else {
				const D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv = swapChain.ResolveCurrentRtvHandle();
				commandList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
			}
			ID3D12DescriptorHeap* descriptorHeaps[] = { targets[sourceIndex].ResolveDescriptorHeap() };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
			pipeline.BindPostProcess(commandList, passIndex, targets[sourceIndex].ResolveGpuHandle());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(3, 1, 0, 0);
			if (!lastPass) {
				ID3D12Resource* target = targets[targetIndex].ResolveResource();
				Dx12Barrier::Transition(commandList, enhancedCommandList, target,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
		}
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	}

	void Dx12PostProcess::Reset() {
		ClearEffects();
		depth.Reset();
		depthDsvHeap.Reset();
		focusHistory[0].Reset();
		focusHistory[1].Reset();
		focusHistoryRtvHeap.Reset();
		focusHistorySrvHeap.Reset();
		targets.clear();
		focusHistoryInitialized = false;
		focusHistoryIndex = 0;
	}
}
