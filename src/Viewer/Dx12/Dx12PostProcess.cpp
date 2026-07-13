#include "Viewer/Dx12/Dx12PostProcess.h"

#include "Viewer/Dx12/Helper/Dx12Barrier.h"
#include "Viewer/Dx12/Helper/Dx12CommandContext.h"
#include "Viewer/Dx12/Helper/Dx12PipelineBuilder.h"
#include "Viewer/Dx12/Helper/Dx12SwapChain.h"
#include "Viewer/Shader/PostProcessInputLayout.h"
#include "Viewer/Viewer.h"

#include <iostream>
#include <limits>
#include <utility>

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
		ResetHistory();
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
		srvHeapDesc.NumDescriptors = PostProcessInputLayout::requiredTextureCount;
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
		for (size_t index = 0; index < 2; index++) {
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

	void Dx12PostProcess::UpdateFocusHistoryShaderResources(
		const Dx12Device& sourceDevice, const size_t readIndex) const {
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
		const Dx12CommandContext& commandContext, const D3D12_GPU_VIRTUAL_ADDRESS frameDataAddress,
		const int width, const int height) {
		if (!focusHistoryPipelineState || !focusHistory[0] || !focusHistory[1] || !focusHistorySrvHeap)
			return;
		const size_t readIndex = focusHistoryIndex;
		const size_t writeIndex = ResolveNextHistoryIndex(focusHistoryIndex);
		UpdateFocusHistoryShaderResources(sourceDevice, readIndex);
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, focusHistory[writeIndex].Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = ResolveFocusHistoryRtvHandle(sourceDevice, writeIndex);
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
		ApplyViewportAndScissor(commandList, 1, 1);
		ID3D12DescriptorHeap* descriptorHeaps[] = { focusHistorySrvHeap.Get() };
		commandList->SetDescriptorHeaps(1, descriptorHeaps);
		BindFocusHistory(commandList, ResolveFocusHistoryGpuHandle(), frameDataAddress);
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
		const Dx12Device& sourceDevice, const size_t index) const {
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

	bool Dx12PostProcess::CreatePostProcessRootSignature(const Dx12Device& sourceDevice) {
		D3D12_DESCRIPTOR_RANGE srvRange{};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = PostProcessInputLayout::requiredTextureCount;
		srvRange.BaseShaderRegister = PostProcessInputLayout::sceneColorRegister;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		D3D12_ROOT_PARAMETER rootParameters[2]{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[0].Descriptor.ShaderRegister = PostProcessInputLayout::frameDataRegister;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
		D3D12_STATIC_SAMPLER_DESC sampler{};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = PostProcessInputLayout::linearClampSamplerRegister;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.NumParameters = 2;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return Dx12PipelineBuilder::CreateRootSignature(
			sourceDevice, rootSignatureDesc, postProcessRootSignature);
	}

	bool Dx12PostProcess::CreatePipelineState(const Dx12Device& sourceDevice,
		const EffectPassDefinition& pass, const DXGI_FORMAT format,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState) const {
		std::vector<uint8_t> vertexShader;
		std::vector<uint8_t> pixelShader;
		std::string error;
		if (!Dx12PipelineBuilder::CompileShader(
			sourceDevice, pass.shaderPath, pass.vertexEntry, true, vertexShader, error)
			|| !Dx12PipelineBuilder::CompileShader(
				sourceDevice, pass.shaderPath, pass.pixelEntry, false, pixelShader, error)) {
			std::cerr << error;
			return false;
		}
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = postProcessRootSignature.Get();
		pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
		pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
		pipelineDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_NONE);
		pipelineDesc.DepthStencilState.DepthEnable = FALSE;
		pipelineDesc.DepthStencilState.StencilEnable = FALSE;
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = format;
		pipelineDesc.SampleDesc.Count = 1;
		return SUCCEEDED(sourceDevice.device->CreateGraphicsPipelineState(
			&pipelineDesc, IID_PPV_ARGS(&pipelineState)));
	}

	bool Dx12PostProcess::CreatePipelines(const Dx12Device& sourceDevice) {
		ResetPipelines();
		if (!sourceDevice.device)
			return false;
		if (ResolvePasses().empty())
			return true;
		if (!CreatePostProcessRootSignature(sourceDevice))
			return false;
		for (const auto& pass : ResolvePasses()) {
			Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
			if (!CreatePipelineState(sourceDevice, pass, DXGI_FORMAT_R8G8B8A8_UNORM, pipelineState)) {
				ResetPipelines();
				return false;
			}
			postProcessPipelineStates.push_back(std::move(pipelineState));
		}
		const auto* historyPass = ResolveHistoryPass();
		if (historyPass && !CreatePipelineState(
			sourceDevice, *historyPass, DXGI_FORMAT_R32G32B32A32_FLOAT, focusHistoryPipelineState)) {
			ResetPipelines();
			return false;
		}
		return true;
	}

	void Dx12PostProcess::BindFocusHistory(
		ID3D12GraphicsCommandList* commandList, const D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle,
		const D3D12_GPU_VIRTUAL_ADDRESS frameDataAddress) const {
		if (commandList == nullptr || !postProcessRootSignature || !focusHistoryPipelineState)
			return;
		commandList->SetGraphicsRootSignature(postProcessRootSignature.Get());
		commandList->SetPipelineState(focusHistoryPipelineState.Get());
		commandList->SetGraphicsRootConstantBufferView(0, frameDataAddress);
		commandList->SetGraphicsRootDescriptorTable(1, sceneColorHandle);
	}

	void Dx12PostProcess::BindPostProcess(ID3D12GraphicsCommandList* commandList,
		const size_t passIndex, const D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle,
		const D3D12_GPU_VIRTUAL_ADDRESS frameDataAddress) const {
		if (commandList == nullptr || !postProcessRootSignature || passIndex >= postProcessPipelineStates.size())
			return;
		commandList->SetGraphicsRootSignature(postProcessRootSignature.Get());
		commandList->SetPipelineState(postProcessPipelineStates[passIndex].Get());
		commandList->SetGraphicsRootConstantBufferView(0, frameDataAddress);
		commandList->SetGraphicsRootDescriptorTable(1, sceneColorHandle);
	}

	void Dx12PostProcess::ResetPipelines() {
		focusHistoryPipelineState.Reset();
		postProcessPipelineStates.clear();
		postProcessRootSignature.Reset();
	}

	bool Dx12PostProcess::InitializeTargets(const Dx12Device& sourceDevice, const int width, const int height) {
		targets.resize(targetCount);
		for (auto& target : targets) {
			if (!target.Initialize(sourceDevice, width, height))
				return false;
		}
		if (!CreateDepthTarget(sourceDevice, width, height))
			return false;
		const size_t frameDataSize = Dx12Buffer::AlignConstantBufferSize(sizeof(PostProcessFrameData));
		for (auto& buffer : frameDataBuffers) {
			if (!buffer.InitializeUpload(sourceDevice, frameDataSize))
				return false;
		}
		return true;
	}

	bool Dx12PostProcess::Load(
		const Dx12Device& sourceDevice, const std::vector<const EffectDefinition*>& effects) {
		if (!SetEffects(effects))
			return false;
		ResetHistory();
		if (!CreatePipelines(sourceDevice)) {
			ClearPipelines();
			return false;
		}
		return true;
	}

	void Dx12PostProcess::ClearPipelines() {
		ResetPipelines();
		ClearEffects();
		ResetHistory();
	}

	bool Dx12PostProcess::BeginDepthPass(
		ID3D12GraphicsCommandList* commandList, const Dx12CommandContext& commandContext, const int width, const int height) const {
		if (!RequiresDepth() || !depth || commandList == nullptr)
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
		const Dx12SwapChain& swapChain, const int width, const int height,
		const PostProcessFrameData& frameData) {
		if (targets.size() < targetCount) {
			ResolveToBackBuffer(commandList, backBuffer, msaaColor, sourceDevice, commandContext);
			return;
		}
		Dx12Buffer& frameDataBuffer = frameDataBuffers[swapChain.GetFrameIndex() % frameDataBufferCount];
		if (!frameDataBuffer.Write(frameData)) {
			ResolveToBackBuffer(commandList, backBuffer, msaaColor, sourceDevice, commandContext);
			return;
		}
		const D3D12_GPU_VIRTUAL_ADDRESS frameDataAddress = frameDataBuffer.ResolveGpuAddress();
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
		UpdateFocusHistory(commandList, sourceDevice, commandContext, frameDataAddress, width, height);
		ID3D12Resource* focusHistoryResource = focusHistoryPipelineState && focusHistoryInitialized
			? focusHistory[focusHistoryIndex].Get() : nullptr;
		for (const auto& target : targets)
			target.UpdateFocusHistoryShaderResourceView(sourceDevice, focusHistoryResource);
		const size_t passCount = postProcessPipelineStates.size();
		for (size_t passIndex = 0; passIndex < passCount; passIndex++) {
			const PostProcessPassRoute route = ResolvePingPongRoute(passIndex, passCount);
			if (!route.lastPass) {
				ID3D12Resource* target = targets[route.targetIndex].ResolveResource();
				Dx12Barrier::Transition(commandList, enhancedCommandList, target,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				const D3D12_CPU_DESCRIPTOR_HANDLE targetRtv = targets[route.targetIndex].ResolveRtvHandle();
				commandList->OMSetRenderTargets(1, &targetRtv, FALSE, nullptr);
			} else {
				const D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv = swapChain.ResolveCurrentRtvHandle();
				commandList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
			}
			ID3D12DescriptorHeap* descriptorHeaps[] = { targets[route.sourceIndex].ResolveDescriptorHeap() };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
			BindPostProcess(commandList, passIndex,
				targets[route.sourceIndex].ResolveGpuHandle(), frameDataAddress);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(3, 1, 0, 0);
			if (!route.lastPass) {
				ID3D12Resource* target = targets[route.targetIndex].ResolveResource();
				Dx12Barrier::Transition(commandList, enhancedCommandList, target,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
		}
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	}

	void Dx12PostProcess::ResetHistory() {
		focusHistoryInitialized = false;
		focusHistoryIndex = 0;
	}

	void Dx12PostProcess::Reset() {
		ResetPipelines();
		ResetHistory();
		depth.Reset();
		depthDsvHeap.Reset();
		focusHistory[0].Reset();
		focusHistory[1].Reset();
		focusHistoryRtvHeap.Reset();
		focusHistorySrvHeap.Reset();
		for (auto& buffer : frameDataBuffers)
			buffer.Reset();
		targets.clear();
	}
}
