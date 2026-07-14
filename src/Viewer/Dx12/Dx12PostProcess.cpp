#include "Viewer/Dx12/Dx12PostProcess.h"

#include "Viewer/Dx12/Helper/Dx12Barrier.h"
#include "Viewer/Dx12/Helper/Dx12PipelineBuilder.h"
#include "Viewer/Dx12/Helper/Dx12SwapChain.h"
#include "Viewer/Shader/PostProcessInputLayout.h"
#include "Viewer/Viewer.h"

#include <iostream>

namespace Chrivent {
	void Dx12PostProcess::ResolveToBackBuffer(ID3D12GraphicsCommandList* commandList,
		ID3D12Resource* backBuffer, ID3D12Resource* msaaColor, const Dx12Device& sourceDevice,
		const Dx12CommandContext& commandContext) {
		if (commandList == nullptr || backBuffer == nullptr || msaaColor == nullptr)
			return;
		const D3D12_RESOURCE_STATES sourceState = sourceDevice.msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;
		const D3D12_RESOURCE_STATES destinationState = sourceDevice.msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_DEST : D3D12_RESOURCE_STATE_COPY_DEST;
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
		if (commandList == nullptr)
			return;
		D3D12_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(width);
		viewport.Height = static_cast<float>(height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &viewport);
		D3D12_RECT scissor{};
		scissor.right = width;
		scissor.bottom = height;
		commandList->RSSetScissorRects(1, &scissor);
	}

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
		inputDescriptorHeaps.resize(ResolvePassRoutes().size() * frameDataBufferCount);
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
		const Dx12PostProcessResource& resource = resources[input.resourceIndex];
		const size_t index = ResolveResourcePlans()[input.resourceIndex].lifetime
			== EffectResourceLifetime::History ? resource.historyIndex : 0;
		format = resource.targets[index].ResolveFormat();
		return resource.targets[index].ResolveResource();
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
			auto& [targets, historyIndex, historyInitialized] = resources[index];
			if (plans[index].lifetime != EffectResourceLifetime::History || historyInitialized)
				continue;
			for (auto& target : targets) {
				Dx12Barrier::Transition(commandList, enhancedCommandList, target.ResolveResource(),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				commandList->ClearRenderTargetView(target.ResolveRtvHandle(), clearColor, 0, nullptr);
				Dx12Barrier::Transition(commandList, enhancedCommandList, target.ResolveResource(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			historyIndex = 0;
			historyInitialized = true;
		}
	}

	bool Dx12PostProcess::CreatePostProcessRootSignature(const Dx12Device& sourceDevice) {
		D3D12_DESCRIPTOR_RANGE srvRange{};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = PostProcessInputLayout::maxTextureCount;
		srvRange.BaseShaderRegister = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		D3D12_ROOT_PARAMETER rootParameters[2]{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[0].Descriptor.ShaderRegister = PostProcessInputLayout::frameDataRegister;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
		D3D12_STATIC_SAMPLER_DESC samplers[3]{};
		for (UINT index = 0; index < 3; index++) {
			samplers[index].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplers[index].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplers[index].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplers[index].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplers[index].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			samplers[index].MaxLOD = D3D12_FLOAT32_MAX;
			samplers[index].ShaderRegister = index;
			samplers[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
		D3D12_ROOT_SIGNATURE_DESC description;
		description.NumParameters = 2;
		description.pParameters = rootParameters;
		description.NumStaticSamplers = 3;
		description.pStaticSamplers = samplers;
		description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return Dx12PipelineBuilder::CreateRootSignature(sourceDevice, description, postProcessRootSignature);
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
		D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
		description.pRootSignature = postProcessRootSignature.Get();
		description.VS = { vertexShader.data(), vertexShader.size() };
		description.PS = { pixelShader.data(), pixelShader.size() };
		description.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		description.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(description.RasterizerState, D3D12_CULL_MODE_NONE);
		description.DepthStencilState.DepthEnable = FALSE;
		description.DepthStencilState.StencilEnable = FALSE;
		description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		description.NumRenderTargets = 1;
		description.RTVFormats[0] = format;
		description.SampleDesc.Count = 1;
		return SUCCEEDED(sourceDevice.device->CreateGraphicsPipelineState(
			&description, IID_PPV_ARGS(&pipelineState)));
	}

	bool Dx12PostProcess::CreatePipelines(const Dx12Device& sourceDevice) {
		ResetPipelines();
		if (!sourceDevice.device || ResolvePasses().empty())
			return ResolvePasses().empty();
		if (!CreatePostProcessRootSignature(sourceDevice))
			return false;
		const auto& passes = ResolvePasses();
		const auto& routes = ResolvePassRoutes();
		for (size_t index = 0; index < passes.size(); index++) {
			Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
			DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
			if (routes[index].outputKind == PostProcessOutputKind::Resource) {
				const Dx12PostProcessTarget* target = ResolveOutputTarget(routes[index]);
				if (target == nullptr)
					return false;
				format = target->ResolveFormat();
			}
			if (!CreatePipelineState(sourceDevice, passes[index], format, pipeline)) {
				ResetPipelines();
				return false;
			}
			postProcessPipelineStates.emplace_back(std::move(pipeline));
		}
		return true;
	}

	ID3D12DescriptorHeap* Dx12PostProcess::ResolveInputDescriptorHeap(
		const size_t frameIndex, const size_t passIndex) const {
		const size_t index = frameIndex % frameDataBufferCount * ResolvePassRoutes().size() + passIndex;
		return index < inputDescriptorHeaps.size() ? inputDescriptorHeaps[index].Get() : nullptr;
	}

	Dx12PostProcessTarget* Dx12PostProcess::ResolveOutputTarget(const PostProcessPassRoute& route) {
		if (route.outputKind != PostProcessOutputKind::Resource || route.outputResourceIndex >= resources.size())
			return nullptr;
		Dx12PostProcessResource& resource = resources[route.outputResourceIndex];
		const size_t index = ResolveResourcePlans()[route.outputResourceIndex].lifetime
			== EffectResourceLifetime::History ? ResolveNextHistoryIndex(resource.historyIndex) : 0;
		return &resource.targets[index];
	}

	void Dx12PostProcess::ResolveOutputExtent(
		const PostProcessPassRoute& route, int& width, int& height) const {
		width = targetWidth;
		height = targetHeight;
		if (route.outputKind == PostProcessOutputKind::Present
			|| route.outputResourceIndex >= ResolveResourcePlans().size())
			return;
		const PostProcessResourcePlan& plan = ResolveResourcePlans()[route.outputResourceIndex];
		width = ResolveResourceExtent(targetWidth, plan, true);
		height = ResolveResourceExtent(targetHeight, plan, false);
	}

	void Dx12PostProcess::AdvanceHistory(const PostProcessPassRoute& route) {
		if (route.outputKind != PostProcessOutputKind::Resource
			|| route.outputResourceIndex >= resources.size()
			|| ResolveResourcePlans()[route.outputResourceIndex].lifetime != EffectResourceLifetime::History)
			return;
		Dx12PostProcessResource& resource = resources[route.outputResourceIndex];
		resource.historyIndex = ResolveNextHistoryIndex(resource.historyIndex);
		resource.historyInitialized = true;
	}

	void Dx12PostProcess::ResetPipelines() {
		postProcessPipelineStates.clear();
		postProcessRootSignature.Reset();
	}

	void Dx12PostProcess::ResetEffectResources() {
		inputDescriptorHeaps.clear();
		resources.clear();
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
			|| !CreateInputDescriptorHeaps(sourceDevice))
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
		ResetPipelines();
		ResetEffectResources();
		if (!SetEffects(effects)
			|| (targetWidth > 0 && targetHeight > 0 && !CreateEffectResources(sourceDevice))
			|| !CreateInputDescriptorHeaps(sourceDevice)
			|| !CreatePipelines(sourceDevice)) {
			ClearPipelines();
			return false;
		}
		ResetHistory();
		return true;
	}

	void Dx12PostProcess::ClearPipelines() {
		ResetPipelines();
		ResetEffectResources();
		ClearEffects();
	}

	bool Dx12PostProcess::BeginDepthPass(ID3D12GraphicsCommandList* commandList,
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
		ApplyViewportAndScissor(commandList, width, height);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		return true;
	}

	void Dx12PostProcess::EndDepthPass(
		ID3D12GraphicsCommandList* commandList, const Dx12CommandContext& commandContext) const {
		if (!depth || commandList == nullptr)
			return;
		Dx12Barrier::Transition(commandList, commandContext.GetEnhancedCommandList().Get(), depth.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		if (RequiresVelocity())
			Dx12Barrier::Transition(commandList, commandContext.GetEnhancedCommandList().Get(),
				sceneVelocity.ResolveResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void Dx12PostProcess::Draw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer,
		ID3D12Resource* msaaColor, const Dx12Device& sourceDevice,
		const Dx12CommandContext& commandContext, const Dx12SwapChain& swapChain,
		const int width, const int height, const PostProcessFrameData& frameData) {
		if (!HasEffects() || commandList == nullptr || !sceneColor.ResolveResource()) {
			ResolveToBackBuffer(commandList, backBuffer, msaaColor, sourceDevice, commandContext);
			return;
		}
		const size_t frameIndex = swapChain.GetFrameIndex() % frameDataBufferCount;
		const Dx12Buffer& frameDataBuffer = frameDataBuffers[frameIndex];
		if (!frameDataBuffer.Write(frameData)) {
			ResolveToBackBuffer(commandList, backBuffer, msaaColor, sourceDevice, commandContext);
			return;
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
		InitializeHistories(commandList, commandContext);
		const auto& routes = ResolvePassRoutes();
		const D3D12_GPU_VIRTUAL_ADDRESS frameDataAddress = frameDataBuffer.ResolveGpuAddress();
		for (size_t passIndex = 0; passIndex < routes.size(); passIndex++) {
			const PostProcessPassRoute& route = routes[passIndex];
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
			ApplyViewportAndScissor(commandList, outputWidth, outputHeight);
			UpdateInputDescriptors(sourceDevice, frameIndex, passIndex);
			ID3D12DescriptorHeap* heap = ResolveInputDescriptorHeap(frameIndex, passIndex);
			commandList->SetDescriptorHeaps(1, &heap);
			commandList->SetGraphicsRootSignature(postProcessRootSignature.Get());
			commandList->SetPipelineState(postProcessPipelineStates[passIndex].Get());
			commandList->SetGraphicsRootConstantBufferView(0, frameDataAddress);
			commandList->SetGraphicsRootDescriptorTable(1, heap->GetGPUDescriptorHandleForHeapStart());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(3, 1, 0, 0);
			if (outputTarget != nullptr)
				Dx12Barrier::Transition(commandList, enhancedCommandList, outputTarget->ResolveResource(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			AdvanceHistory(route);
		}
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	}

	void Dx12PostProcess::ResetHistory() {
		for (auto& resource : resources) {
			resource.historyIndex = 0;
			resource.historyInitialized = false;
		}
	}

	void Dx12PostProcess::Reset() {
		ResetPipelines();
		ResetEffectResources();
		depth.Reset();
		depthDsvHeap.Reset();
		sceneVelocity.Reset();
		sceneColor.Reset();
		for (auto& buffer : frameDataBuffers)
			buffer.Reset();
		targetWidth = 0;
		targetHeight = 0;
		ClearEffects();
	}
}
