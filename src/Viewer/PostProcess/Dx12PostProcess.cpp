#include "Viewer/PostProcess/Dx12PostProcess.h"

#include "Viewer/Synchronization/Dx12Barrier.h"
#include "Viewer/SwapChain/Dx12SwapChain.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Viewer/Viewer.h"

#include <iostream>

namespace Chrivent {
	bool Dx12PostProcess::CreateDepthTarget(
		const Dx12Device& sourceDevice, const int width, const int height) {
		depth.Reset();
		depthDsvHeap.Reset();
		if (!sourceDevice.device || width <= 0 || height <= 0)
			return false;
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC description{};
		description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		description.Width = width;
		description.Height = height;
		description.DepthOrArraySize = 1;
		description.MipLevels = 1;
		description.Format = DXGI_FORMAT_R24G8_TYPELESS;
		description.SampleDesc.Count = 1;
		description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clearValue.DepthStencil.Depth = 1.0f;
		if (FAILED(sourceDevice.device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
			&description, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&depth))))
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
		heapDescription.NumDescriptors = 1;
		heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		if (FAILED(sourceDevice.device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&depthDsvHeap))))
			return false;
		D3D12_DEPTH_STENCIL_VIEW_DESC viewDescription{};
		viewDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		viewDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		sourceDevice.device->CreateDepthStencilView(
			depth.Get(), &viewDescription, depthDsvHeap->GetCPUDescriptorHandleForHeapStart());
		return true;
	}

	bool Dx12PostProcess::CreateEffectResources(const Dx12Device& sourceDevice) {
		ResetEffectResources();
		const auto& plans = ResolveResourcePlans();
		resources.resize(plans.size());
		for (size_t resourceIndex = 0; resourceIndex < plans.size(); resourceIndex++) {
			const PostProcessResourcePlan& plan = plans[resourceIndex];
			const size_t targetCount = plan.lifetime == EffectResourceLifetime::History ? 2 : 1;
			const DXGI_FORMAT format = plan.format == EffectTextureFormat::Rgba8Unorm
				? DXGI_FORMAT_R8G8B8A8_UNORM
				: plan.format == EffectTextureFormat::Rgba16Float
					? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT;
			for (size_t index = 0; index < targetCount; index++) {
				if (!resources[resourceIndex].targets[index].Initialize(sourceDevice,
					ResolveResourceExtent(targetWidth, plan, true),
					ResolveResourceExtent(targetHeight, plan, false), format))
					return false;
			}
		}
		ResetHistory();
		return true;
	}

	bool Dx12PostProcess::CreateInputDescriptorHeaps(const Dx12Device& sourceDevice) {
		inputDescriptorHeaps.clear();
		if (!sourceDevice.device)
			return false;
		inputDescriptorHeaps.resize(ResolvePassRoutes().size() * FrameBuffering::dx12BufferCount);
		D3D12_DESCRIPTOR_HEAP_DESC description{};
		description.NumDescriptors = PostProcessInputLayout::maxTextureCount;
		description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		for (auto& heap : inputDescriptorHeaps) {
			if (FAILED(sourceDevice.device->CreateDescriptorHeap(&description, IID_PPV_ARGS(&heap))))
				return false;
		}
		return true;
	}

	bool Dx12PostProcess::CreateParameterDataBuffers(const Dx12Device& sourceDevice) {
		for (auto& buffer : parameterDataBuffers)
			buffer.Reset();
		if (!sourceDevice.device)
			return false;
		const size_t passCount = ResolvePassRoutes().size();
		if (passCount == 0)
			return true;
		const size_t stride = Dx12Buffer::AlignConstantBufferSize(sizeof(PostProcessParameterData));
		for (auto& buffer : parameterDataBuffers) {
			if (!buffer.InitializeUpload(sourceDevice, stride * passCount))
				return false;
		}
		return true;
	}

	ID3D12Resource* Dx12PostProcess::ResolveInputResource(
		const PostProcessPassInputRoute& input, DXGI_FORMAT& format) const {
		if (input.kind == PostProcessInputKind::SceneColor) {
			format = sceneColor.ResolveFormat();
			return sceneColor.ResolveResource();
		}
		if (input.kind == PostProcessInputKind::SceneDepth) {
			format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			return depth.Get();
		}
		if (input.kind == PostProcessInputKind::SceneVelocity) {
			format = sceneVelocity.ResolveFormat();
			return sceneVelocity.ResolveResource();
		}
		if (input.resourceIndex >= resources.size()) {
			format = sceneColor.ResolveFormat();
			return sceneColor.ResolveResource();
		}
		const auto& [targets] = resources[input.resourceIndex];
		const size_t index = ResolveResourceReadIndex(input.resourceIndex);
		format = targets[index].ResolveFormat();
		return targets[index].ResolveResource();
	}

	void Dx12PostProcess::UpdateInputDescriptors(
		const Dx12Device& sourceDevice, const size_t frameIndex, const size_t passIndex) const {
		ID3D12DescriptorHeap* heap = ResolveInputDescriptorHeap(frameIndex, passIndex);
		if (!sourceDevice.device || heap == nullptr || passIndex >= ResolvePassRoutes().size())
			return;
		ID3D12Device* device = sourceDevice.device.Get();
		const UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
		std::vector<const PostProcessPassInputRoute*> slots(PostProcessInputLayout::maxTextureCount);
		for (const auto& input : ResolvePassRoutes()[passIndex].inputs)
			slots[input.slot] = &input;
		for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++) {
			DXGI_FORMAT format = sceneColor.ResolveFormat();
			ID3D12Resource* resource = sceneColor.ResolveResource();
			if (slots[slot] != nullptr)
				resource = ResolveInputResource(*slots[slot], format);
			D3D12_SHADER_RESOURCE_VIEW_DESC description{};
			description.Format = format;
			description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			description.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(resource, &description, handle);
			handle.ptr += increment;
		}
	}

	void Dx12PostProcess::InitializeHistories(ID3D12GraphicsCommandList* commandList,
		const Dx12CommandContext& commandContext) {
		if (commandList == nullptr)
			return;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		constexpr float clearColor[4]{};
		const auto& plans = ResolveResourcePlans();
		for (size_t index = 0; index < resources.size() && index < plans.size(); index++) {
			auto& targets = resources[index].targets;
			if (!NeedsHistoryInitialization(index))
				continue;
			for (auto& target : targets) {
				Dx12Barrier::Transition(commandList, enhancedCommandList, target.ResolveResource(),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				commandList->ClearRenderTargetView(target.ResolveRtvHandle(), clearColor, 0, nullptr);
				Dx12Barrier::Transition(commandList, enhancedCommandList, target.ResolveResource(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			MarkHistoryInitialized(index);
		}
	}

	bool Dx12PostProcess::CreatePipelines(const Dx12Device& sourceDevice) {
		const auto& passes = ResolvePasses();
		const auto& routes = ResolvePassRoutes();
		std::vector<DXGI_FORMAT> formats;
		formats.reserve(passes.size());
		for (size_t index = 0; index < passes.size(); index++) {
			DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
			if (routes[index].outputKind == PostProcessOutputKind::Resource) {
				const Dx12PostProcessTarget* target = ResolveOutputTarget(routes[index]);
				if (target == nullptr)
					return false;
				format = target->ResolveFormat();
			}
			formats.emplace_back(format);
		}
		return pipelines.Initialize(sourceDevice, passes, formats);
	}

	ID3D12DescriptorHeap* Dx12PostProcess::ResolveInputDescriptorHeap(
		const size_t frameIndex, const size_t passIndex) const {
		const size_t index = frameIndex % FrameBuffering::dx12BufferCount * ResolvePassRoutes().size() + passIndex;
		return index < inputDescriptorHeaps.size() ? inputDescriptorHeaps[index].Get() : nullptr;
	}

	Dx12PostProcessTarget* Dx12PostProcess::ResolveOutputTarget(const PostProcessPassRoute& route) {
		if (route.outputKind != PostProcessOutputKind::Resource || route.outputResourceIndex >= resources.size())
			return nullptr;
		auto& [targets] = resources[route.outputResourceIndex];
		return &targets[ResolveResourceWriteIndex(route.outputResourceIndex)];
	}

	void Dx12PostProcess::ResetEffectResources() {
		inputDescriptorHeaps.clear();
		resources.clear();
		for (auto& buffer : parameterDataBuffers)
			buffer.Reset();
	}

	bool Dx12PostProcess::InitializeTargets(
		const Dx12Device& sourceDevice, const int width, const int height) {
		if (!sourceDevice.device || width <= 0 || height <= 0)
			return false;
		targetWidth = width;
		targetHeight = height;
		sceneColor.Reset();
		sceneVelocity.Reset();
		ResetEffectResources();
		depth.Reset();
		depthDsvHeap.Reset();
		for (auto& buffer : frameDataBuffers)
			buffer.Reset();
		if (!sceneColor.Initialize(sourceDevice, width, height, DXGI_FORMAT_R8G8B8A8_UNORM)
			|| !sceneVelocity.Initialize(sourceDevice, width, height, DXGI_FORMAT_R16G16_FLOAT)
			|| !CreateDepthTarget(sourceDevice, width, height)
			|| !CreateEffectResources(sourceDevice)
			|| !CreateInputDescriptorHeaps(sourceDevice)
			|| !CreateParameterDataBuffers(sourceDevice))
			return false;
		const size_t frameDataSize = Dx12Buffer::AlignConstantBufferSize(sizeof(PostProcessFrameData));
		for (auto& buffer : frameDataBuffers) {
			if (!buffer.InitializeUpload(sourceDevice, frameDataSize))
				return false;
		}
		return true;
	}

	bool Dx12PostProcess::Load(
		const Dx12Device& sourceDevice, const std::vector<const EffectRuntimeDefinition*>& effects) {
		Dx12PostProcess candidate;
		candidate.targetWidth = targetWidth;
		candidate.targetHeight = targetHeight;
		if (!candidate.SetEffects(effects)
			|| (targetWidth > 0 && targetHeight > 0 && !candidate.CreateEffectResources(sourceDevice))
			|| !candidate.CreateInputDescriptorHeaps(sourceDevice)
			|| !candidate.CreateParameterDataBuffers(sourceDevice)
			|| !candidate.CreatePipelines(sourceDevice))
			return false;
		SwapExecutionPlan(candidate);
		resources.swap(candidate.resources);
		inputDescriptorHeaps.swap(candidate.inputDescriptorHeaps);
		pipelines.Swap(candidate.pipelines);
		for (size_t index = 0; index < FrameBuffering::dx12BufferCount; index++)
			parameterDataBuffers[index].Swap(candidate.parameterDataBuffers[index]);
		return true;
	}

	bool Dx12PostProcess::BeginSceneInputPass(ID3D12GraphicsCommandList* commandList,
		const Dx12CommandContext& commandContext, const int width, const int height) const {
		if ((!RequiresDepth() && !RequiresVelocity()) || !depth || commandList == nullptr)
			return false;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, depth.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		if (RequiresVelocity())
			Dx12Barrier::Transition(commandList, enhancedCommandList, sceneVelocity.ResolveResource(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthDsvHeap->GetCPUDescriptorHandleForHeapStart();
		const D3D12_CPU_DESCRIPTOR_HANDLE velocityHandle = sceneVelocity.ResolveRtvHandle();
		commandList->OMSetRenderTargets(RequiresVelocity() ? 1 : 0,
			RequiresVelocity() ? &velocityHandle : nullptr, FALSE, &dsvHandle);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		if (RequiresVelocity()) {
			constexpr float velocityClear[4]{};
			commandList->ClearRenderTargetView(velocityHandle, velocityClear, 0, nullptr);
		}
		Dx12CommandContext::ApplyViewportAndScissor(commandList, width, height);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		return true;
	}

	bool Dx12PostProcess::EndSceneInputPass(
		ID3D12GraphicsCommandList* commandList, const Dx12CommandContext& commandContext) const {
		if (!depth || commandList == nullptr)
			return false;
		Dx12Barrier::Transition(commandList, commandContext.GetEnhancedCommandList().Get(), depth.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		if (RequiresVelocity())
			Dx12Barrier::Transition(commandList, commandContext.GetEnhancedCommandList().Get(),
				sceneVelocity.ResolveResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		return true;
	}

	bool Dx12PostProcess::Draw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer,
		const Dx12MsaaColorBuffer& msaaColorBuffer, const Dx12Device& sourceDevice,
		const Dx12CommandContext& commandContext, const Dx12SwapChain& swapChain,
		const int width, const int height, const PostProcessFrameData& frameData) {
		ID3D12Resource* msaaColor = msaaColorBuffer.ResolveResource();
		if (!HasEffects() || commandList == nullptr || !sceneColor.ResolveResource()) {
			return msaaColorBuffer.ResolveToBackBuffer(
				commandList, commandContext.GetEnhancedCommandList().Get(), backBuffer);
		}
		const size_t frameIndex = swapChain.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const Dx12Buffer& frameDataBuffer = frameDataBuffers[frameIndex];
		const Dx12Buffer& parameterDataBuffer = parameterDataBuffers[frameIndex];
		const size_t parameterStride = Dx12Buffer::AlignConstantBufferSize(sizeof(PostProcessParameterData));
		if (!frameDataBuffer.Write(frameData) || !parameterDataBuffer.IsInitialized()) {
			msaaColorBuffer.ResolveToBackBuffer(
				commandList, commandContext.GetEnhancedCommandList().Get(), backBuffer);
			return false;
		}
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		const D3D12_RESOURCE_STATES sourceState = sourceDevice.msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;
		const D3D12_RESOURCE_STATES destinationState = sourceDevice.msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_DEST : D3D12_RESOURCE_STATE_COPY_DEST;
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			D3D12_RESOURCE_STATE_RENDER_TARGET, sourceState);
		Dx12Barrier::Transition(commandList, enhancedCommandList, sceneColor.ResolveResource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, destinationState);
		if (sourceDevice.msaaSampleCount > 1)
			commandList->ResolveSubresource(
				sceneColor.ResolveResource(), 0, msaaColor, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			commandList->CopyResource(sceneColor.ResolveResource(), msaaColor);
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			sourceState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Dx12Barrier::Transition(commandList, enhancedCommandList, sceneColor.ResolveResource(),
			destinationState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		BeginHistoryFrame();
		InitializeHistories(commandList, commandContext);
		const auto& routes = ResolvePassRoutes();
		const D3D12_GPU_VIRTUAL_ADDRESS frameDataAddress = frameDataBuffer.ResolveGpuAddress();
		for (size_t passIndex = 0; passIndex < routes.size(); passIndex++) {
			const PostProcessPassRoute& route = routes[passIndex];
			const size_t parameterOffset = passIndex * parameterStride;
			if (!parameterDataBuffer.Write(route.parameters, parameterOffset)) {
				msaaColorBuffer.ResolveToBackBuffer(
					commandList, commandContext.GetEnhancedCommandList().Get(), backBuffer);
				DiscardHistoryFrame();
				return false;
			}
			const Dx12PostProcessTarget* outputTarget = ResolveOutputTarget(route);
			if (outputTarget != nullptr) {
				Dx12Barrier::Transition(commandList, enhancedCommandList, outputTarget->ResolveResource(),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				const D3D12_CPU_DESCRIPTOR_HANDLE rtv = outputTarget->ResolveRtvHandle();
				commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
			} else {
				const D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain.ResolveCurrentRtvHandle();
				commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
			}
			int outputWidth = width;
			int outputHeight = height;
			ResolveOutputExtent(route, outputWidth, outputHeight);
			Dx12CommandContext::ApplyViewportAndScissor(commandList, outputWidth, outputHeight);
			UpdateInputDescriptors(sourceDevice, frameIndex, passIndex);
			ID3D12DescriptorHeap* heap = ResolveInputDescriptorHeap(frameIndex, passIndex);
			commandList->SetDescriptorHeaps(1, &heap);
			commandList->SetGraphicsRootSignature(pipelines.ResolveRootSignature());
			commandList->SetPipelineState(pipelines.ResolvePipelineState(passIndex));
			commandList->SetGraphicsRootConstantBufferView(0, frameDataAddress);
			commandList->SetGraphicsRootConstantBufferView(
				1, parameterDataBuffer.ResolveGpuAddress() + parameterOffset);
			commandList->SetGraphicsRootDescriptorTable(2, heap->GetGPUDescriptorHandleForHeapStart());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(3, 1, 0, 0);
			if (outputTarget != nullptr)
				Dx12Barrier::Transition(commandList, enhancedCommandList, outputTarget->ResolveResource(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			AdvanceHistory(route);
		}
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		return true;
	}

	void Dx12PostProcess::ResetResources() {
		pipelines.Reset();
		ResetEffectResources();
		depth.Reset();
		depthDsvHeap.Reset();
		sceneVelocity.Reset();
		sceneColor.Reset();
		for (auto& buffer : frameDataBuffers)
			buffer.Reset();
		for (auto& buffer : parameterDataBuffers)
			buffer.Reset();
		targetWidth = 0;
		targetHeight = 0;
	}
}
