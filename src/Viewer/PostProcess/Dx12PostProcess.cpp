#include "Viewer/PostProcess/Dx12PostProcess.h"

#include "Viewer/Synchronization/Dx12Barrier.h"
#include "Viewer/SwapChain/Dx12SwapChain.h"
#include "Viewer/PostProcess/PostProcessFrameData.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"

#include <limits>
#include <utility>

namespace Chrivent {
	GraphicsResult<void> Dx12PostProcess::CreateDepthTarget(
		const Dx12Device& sourceDevice, const int width, const int height) {
		depth.Reset();
		depthDsvHeap.Reset();
		if (!sourceDevice.GetDevice() || width <= 0 || height <= 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "후처리 depth target 생성",
				"DirectX 12 device 또는 depth target 크기가 올바르지 않습니다"));
		}
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
		HRESULT result = sourceDevice.GetDevice()->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &description,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&depth));
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 depth target 생성",
				"DirectX 12 후처리 depth target을 만들지 못했습니다", result, true));
		}
		D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
		heapDescription.NumDescriptors = 1;
		heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		result = sourceDevice.GetDevice()->CreateDescriptorHeap(
			&heapDescription, IID_PPV_ARGS(&depthDsvHeap));
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 depth descriptor heap 생성",
				"DirectX 12 후처리 depth descriptor heap을 만들지 못했습니다", result, true));
		}
		D3D12_DEPTH_STENCIL_VIEW_DESC viewDescription{};
		viewDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		viewDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		sourceDevice.GetDevice()->CreateDepthStencilView(
			depth.Get(), &viewDescription, depthDsvHeap->GetCPUDescriptorHandleForHeapStart());
		return {};
	}

	GraphicsResult<void> Dx12PostProcess::CreateEffectResources(const Dx12Device& sourceDevice) {
		ResetEffectResources();
		const auto& plans = GetResourcePlans();
		resources.resize(plans.size());
		for (size_t resourceIndex = 0; resourceIndex < plans.size(); resourceIndex++) {
			const PostProcessResourcePlan& plan = plans[resourceIndex];
			const size_t targetCount = plan.lifetime == EffectResourceLifetime::History ? 2 : 1;
			const DXGI_FORMAT format = plan.format == EffectTextureFormat::Rgba8Unorm
				? DXGI_FORMAT_R8G8B8A8_UNORM
				: plan.format == EffectTextureFormat::Rgba16Float
					? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT;
			for (size_t index = 0; index < targetCount; index++) {
				const auto result = resources[resourceIndex].targets[index].Initialize(sourceDevice,
					ResolveResourceExtent(targetWidth, plan, true),
					ResolveResourceExtent(targetHeight, plan, false), format);
				if (!result)
					return std::unexpected(result.error());
			}
		}
		ResetHistory();
		return {};
	}

	GraphicsResult<void> Dx12PostProcess::CreateInputDescriptorHeaps(const Dx12Device& sourceDevice) {
		inputDescriptorHeaps.clear();
		inputDescriptorSize = 0;
		if (!sourceDevice.GetDevice()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 input descriptor heap 생성",
				"DirectX 12 device를 사용할 수 없습니다"));
		}
		const size_t passCount = GetPassRoutes().size();
		if (passCount == 0)
			return {};
		if (passCount > std::numeric_limits<UINT>::max() / PostProcessInputLayout::maxTextureCount) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ContractViolation, "후처리 input descriptor heap 생성",
				"후처리 패스 수가 DirectX 12 descriptor 개수 범위를 벗어났습니다"));
		}
		inputDescriptorHeaps.resize(FrameBuffering::dx12BufferCount);
		inputDescriptorStates.resize(
			FrameBuffering::dx12BufferCount * passCount * PostProcessInputLayout::maxTextureCount);
		D3D12_DESCRIPTOR_HEAP_DESC description{};
		description.NumDescriptors = static_cast<UINT>(
			passCount * PostProcessInputLayout::maxTextureCount);
		description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		for (auto& heap : inputDescriptorHeaps) {
			const HRESULT result = sourceDevice.GetDevice()->CreateDescriptorHeap(
				&description, IID_PPV_ARGS(&heap));
			if (FAILED(result)) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 input descriptor heap 생성",
					"DirectX 12 후처리 input descriptor heap을 만들지 못했습니다", result, true));
			}
		}
		inputDescriptorSize = sourceDevice.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		return {};
	}

	GraphicsResult<void> Dx12PostProcess::CreateParameterDataBuffers(const Dx12Device& sourceDevice) {
		for (auto& buffer : parameterDataBuffers)
			buffer.Reset();
		if (!sourceDevice.GetDevice()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 parameter buffer 생성",
				"DirectX 12 device를 사용할 수 없습니다"));
		}
		const size_t passCount = GetPassRoutes().size();
		if (passCount == 0)
			return {};
		const size_t stride = Dx12Buffer::AlignConstantBufferSize(sizeof(PostProcessParameterData));
		for (auto& buffer : parameterDataBuffers) {
			if (!buffer.InitializeUpload(sourceDevice, stride * passCount)) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 parameter buffer 생성",
					"DirectX 12 후처리 parameter upload buffer를 만들지 못했습니다"));
			}
		}
		return {};
	}

	ID3D12Resource* Dx12PostProcess::ResolveInputResource(
		const PostProcessPassInputRoute& input, DXGI_FORMAT& format) const {
		if (input.kind == PostProcessInputKind::SceneColor) {
			format = sceneColor.GetFormat();
			return sceneColor.GetResource();
		}
		if (input.kind == PostProcessInputKind::SceneDepth) {
			format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			return depth.Get();
		}
		if (input.kind == PostProcessInputKind::SceneVelocity) {
			format = sceneVelocity.GetFormat();
			return sceneVelocity.GetResource();
		}
		if (input.resourceIndex >= resources.size()) {
			format = sceneColor.GetFormat();
			return sceneColor.GetResource();
		}
		const auto& [targets] = resources[input.resourceIndex];
		const size_t index = ResolveResourceReadIndex(input.resourceIndex);
		format = targets[index].GetFormat();
		return targets[index].GetResource();
	}

	void Dx12PostProcess::UpdateInputDescriptors(
		const Dx12Device& sourceDevice, const size_t frameIndex, const size_t passIndex) {
		ID3D12DescriptorHeap* heap = ResolveInputDescriptorHeap(frameIndex);
		if (!sourceDevice.GetDevice() || heap == nullptr || passIndex >= GetPassRoutes().size())
			return;
		ID3D12Device* device = sourceDevice.GetDevice();
		D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += passIndex * PostProcessInputLayout::maxTextureCount * inputDescriptorSize;
		const PostProcessPassInputRoute* slots[PostProcessInputLayout::maxTextureCount]{};
		for (const auto& input : GetPassRoutes()[passIndex].inputs)
			slots[input.slot] = &input;
		for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++) {
			DXGI_FORMAT colFormat = sceneColor.GetFormat();
			ID3D12Resource* colResource = sceneColor.GetResource();
			if (slots[slot] != nullptr)
				colResource = ResolveInputResource(*slots[slot], colFormat);
			const size_t stateIndex =
				(frameIndex % FrameBuffering::dx12BufferCount * GetPassRoutes().size() + passIndex)
				* PostProcessInputLayout::maxTextureCount + slot;
			auto& [resource, format] = inputDescriptorStates[stateIndex];
			if (resource == colResource && format == colFormat) {
				handle.ptr += inputDescriptorSize;
				continue;
			}
			D3D12_SHADER_RESOURCE_VIEW_DESC description{};
			description.Format = colFormat;
			description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			description.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(colResource, &description, handle);
			resource = colResource;
			format = colFormat;
			handle.ptr += inputDescriptorSize;
		}
	}

	void Dx12PostProcess::InitializeHistories(ID3D12GraphicsCommandList* commandList,
		const Dx12CommandContext& commandContext) {
		if (commandList == nullptr)
			return;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		constexpr float clearColor[4]{};
		const auto& plans = GetResourcePlans();
		for (size_t index = 0; index < resources.size() && index < plans.size(); index++) {
			auto& targets = resources[index].targets;
			if (!NeedsHistoryInitialization(index))
				continue;
			for (auto& target : targets) {
				Dx12Barrier::Transition(commandList, enhancedCommandList, target.GetResource(),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				commandList->ClearRenderTargetView(target.GetRtvHandle(), clearColor, 0, nullptr);
				Dx12Barrier::Transition(commandList, enhancedCommandList, target.GetResource(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			MarkHistoryInitialized(index);
		}
	}

	GraphicsResult<void> Dx12PostProcess::CreatePipelines(const Dx12Device& sourceDevice) {
		const auto& passes = GetShaderPrograms();
		const auto& routes = GetPassRoutes();
		std::vector<DXGI_FORMAT> formats;
		formats.reserve(passes.size());
		for (size_t index = 0; index < passes.size(); index++) {
			DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
			if (routes[index].outputKind == PostProcessOutputKind::Resource) {
				const Dx12PostProcessTarget* target = ResolveOutputTarget(routes[index]);
				if (target == nullptr) {
					return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
						GraphicsErrorCode::ContractViolation, "후처리 pipeline 생성",
						"후처리 패스의 DirectX 12 출력 target을 찾지 못했습니다"));
				}
				format = target->GetFormat();
			}
			formats.emplace_back(format);
		}
		std::string error;
		if (!pipelines.Initialize(sourceDevice, passes, formats, error)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::EffectConfigurationFailed, "후처리 pipeline 생성",
				error.empty() ? "DirectX 12 후처리 pipeline을 만들지 못했습니다" : std::move(error)));
		}
		return {};
	}

	ID3D12DescriptorHeap* Dx12PostProcess::ResolveInputDescriptorHeap(const size_t frameIndex) const {
		const size_t index = frameIndex % FrameBuffering::dx12BufferCount;
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
		inputDescriptorStates.clear();
		inputDescriptorSize = 0;
		resources.clear();
		for (auto& buffer : parameterDataBuffers)
			buffer.Reset();
	}

	GraphicsResult<void> Dx12PostProcess::InitializeTargets(
		const Dx12Device& sourceDevice, const int width, const int height) {
		if (!sourceDevice.GetDevice() || width <= 0 || height <= 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "후처리 target 생성",
				"DirectX 12 device 또는 후처리 target 크기가 올바르지 않습니다"));
		}
		targetWidth = width;
		targetHeight = height;
		sceneColor.Reset();
		sceneVelocity.Reset();
		ResetEffectResources();
		depth.Reset();
		depthDsvHeap.Reset();
		for (auto& buffer : frameDataBuffers)
			buffer.Reset();
		auto result = sceneColor.Initialize(sourceDevice, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
		if (result && RequiresVelocity())
			result = sceneVelocity.Initialize(sourceDevice, width, height, DXGI_FORMAT_R16G16_FLOAT);
		if (result && (RequiresDepth() || RequiresVelocity()))
			result = CreateDepthTarget(sourceDevice, width, height);
		if (result)
			result = CreateEffectResources(sourceDevice);
		if (result)
			result = CreateInputDescriptorHeaps(sourceDevice);
		if (result)
			result = CreateParameterDataBuffers(sourceDevice);
		if (!result)
			return std::unexpected(result.error());
		const size_t frameDataSize = Dx12Buffer::AlignConstantBufferSize(sizeof(PostProcessFrameData));
		for (auto& buffer : frameDataBuffers) {
			if (!buffer.InitializeUpload(sourceDevice, frameDataSize)) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 frame buffer 생성",
					"DirectX 12 후처리 frame upload buffer를 만들지 못했습니다"));
			}
		}
		return {};
	}

	void Dx12PostProcess::SwapResources(Dx12PostProcess& other) noexcept {
		std::swap(sceneColor, other.sceneColor);
		std::swap(sceneVelocity, other.sceneVelocity);
		resources.swap(other.resources);
		inputDescriptorHeaps.swap(other.inputDescriptorHeaps);
		inputDescriptorStates.swap(other.inputDescriptorStates);
		depth.Swap(other.depth);
		depthDsvHeap.Swap(other.depthDsvHeap);
		pipelines.Swap(other.pipelines);
		for (size_t index = 0; index < FrameBuffering::dx12BufferCount; index++) {
			frameDataBuffers[index].Swap(other.frameDataBuffers[index]);
			parameterDataBuffers[index].Swap(other.parameterDataBuffers[index]);
		}
		std::swap(targetWidth, other.targetWidth);
		std::swap(targetHeight, other.targetHeight);
		std::swap(inputDescriptorSize, other.inputDescriptorSize);
	}

	GraphicsResult<void> Dx12PostProcess::Configure(const Dx12Device& sourceDevice,
		const int width, const int height,
		const std::vector<const EffectRuntimeDefinition*>& effects) {
		Dx12PostProcess candidate;
		const auto planResult = candidate.SetEffects(effects);
		if (!planResult) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ContractViolation, "후처리 실행 계획 생성", planResult.error()));
		}
		if (candidate.HasEffects()) {
			const auto targetResult = candidate.InitializeTargets(sourceDevice, width, height);
			if (!targetResult)
				return std::unexpected(targetResult.error());
		}
		if (candidate.HasEffects()) {
			const auto pipelineResult = candidate.CreatePipelines(sourceDevice);
			if (!pipelineResult)
				return std::unexpected(pipelineResult.error());
		}
		SwapExecutionPlan(candidate);
		SwapResources(candidate);
		return {};
	}

	GraphicsResult<void> Dx12PostProcess::BeginSceneInputPass(ID3D12GraphicsCommandList* commandList,
		const Dx12CommandContext& commandContext, const int width, const int height) const {
		if ((!RequiresDepth() && !RequiresVelocity()) || !depth || commandList == nullptr) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 시작",
				"DirectX 12 command list 또는 후처리 장면 입력 target이 준비되지 않았습니다"));
		}
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, depth.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		if (RequiresVelocity())
			Dx12Barrier::Transition(commandList, enhancedCommandList, sceneVelocity.GetResource(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthDsvHeap->GetCPUDescriptorHandleForHeapStart();
		const D3D12_CPU_DESCRIPTOR_HANDLE velocityHandle = sceneVelocity.GetRtvHandle();
		commandList->OMSetRenderTargets(RequiresVelocity() ? 1 : 0,
			RequiresVelocity() ? &velocityHandle : nullptr, FALSE, &dsvHandle);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		if (RequiresVelocity()) {
			constexpr float velocityClear[4]{};
			commandList->ClearRenderTargetView(velocityHandle, velocityClear, 0, nullptr);
		}
		Dx12CommandContext::ApplyViewportAndScissor(commandList, width, height);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		return {};
	}

	GraphicsResult<void> Dx12PostProcess::EndSceneInputPass(
		ID3D12GraphicsCommandList* commandList, const Dx12CommandContext& commandContext) const {
		if (!depth || commandList == nullptr) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 종료",
				"DirectX 12 command list 또는 후처리 depth target을 사용할 수 없습니다"));
		}
		Dx12Barrier::Transition(commandList, commandContext.GetEnhancedCommandList().Get(), depth.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		if (RequiresVelocity())
			Dx12Barrier::Transition(commandList, commandContext.GetEnhancedCommandList().Get(),
				sceneVelocity.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		return {};
	}

	GraphicsResult<void> Dx12PostProcess::Draw(
		ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer,
		const Dx12MsaaColorBuffer& msaaColorBuffer, const Dx12Device& sourceDevice,
		const Dx12CommandContext& commandContext, const Dx12SwapChain& swapChain,
		const int width, const int height, const PostProcessFrameData& frameData) {
		ID3D12Resource* msaaColor = msaaColorBuffer.GetResource();
		if (!HasEffects() || commandList == nullptr || !sceneColor.GetResource()
			|| backBuffer == nullptr || msaaColor == nullptr
			|| !IsPassCountCompatible(pipelines.GetCount())) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 효과 draw",
				"DirectX 12 후처리 리소스 또는 실행 계획이 준비되지 않았습니다"));
		}
		const size_t frameIndex = swapChain.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const Dx12Buffer& frameDataBuffer = frameDataBuffers[frameIndex];
		const Dx12Buffer& parameterDataBuffer = parameterDataBuffers[frameIndex];
		const size_t parameterStride = Dx12Buffer::AlignConstantBufferSize(sizeof(PostProcessParameterData));
		if (!frameDataBuffer.Write(frameData) || !parameterDataBuffer.IsInitialized()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandRecordingFailed, "후처리 frame data 기록",
				"DirectX 12 후처리 frame 또는 parameter buffer를 기록하지 못했습니다"));
		}
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		const D3D12_RESOURCE_STATES sourceState = sourceDevice.GetMsaaSampleCount() > 1
			? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;
		const D3D12_RESOURCE_STATES destinationState = sourceDevice.GetMsaaSampleCount() > 1
			? D3D12_RESOURCE_STATE_RESOLVE_DEST : D3D12_RESOURCE_STATE_COPY_DEST;
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			D3D12_RESOURCE_STATE_RENDER_TARGET, sourceState);
		Dx12Barrier::Transition(commandList, enhancedCommandList, sceneColor.GetResource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, destinationState);
		if (sourceDevice.GetMsaaSampleCount() > 1)
			commandList->ResolveSubresource(
				sceneColor.GetResource(), 0, msaaColor, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			commandList->CopyResource(sceneColor.GetResource(), msaaColor);
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			sourceState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Dx12Barrier::Transition(commandList, enhancedCommandList, sceneColor.GetResource(),
			destinationState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		BeginHistoryFrame();
		InitializeHistories(commandList, commandContext);
		const auto& routes = GetPassRoutes();
		ID3D12DescriptorHeap* heap = ResolveInputDescriptorHeap(frameIndex);
		if (heap == nullptr) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 descriptor binding",
				"DirectX 12 후처리 입력 descriptor heap을 사용할 수 없습니다"));
		}
		commandList->SetDescriptorHeaps(1, &heap);
		const D3D12_GPU_VIRTUAL_ADDRESS frameDataAddress = frameDataBuffer.GetGpuAddress();
		for (size_t passIndex = 0; passIndex < routes.size(); passIndex++) {
			const PostProcessPassRoute& route = routes[passIndex];
			const size_t parameterOffset = passIndex * parameterStride;
			if (!parameterDataBuffer.Write(GetParameterData(route), parameterOffset)) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
					GraphicsErrorCode::CommandRecordingFailed, "후처리 parameter 기록",
					"DirectX 12 후처리 pass parameter를 기록하지 못했습니다"));
			}
			const Dx12PostProcessTarget* outputTarget = ResolveOutputTarget(route);
			if (outputTarget != nullptr) {
				Dx12Barrier::Transition(commandList, enhancedCommandList, outputTarget->GetResource(),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				const D3D12_CPU_DESCRIPTOR_HANDLE rtv = outputTarget->GetRtvHandle();
				commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
			} else {
				const D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain.GetCurrentRtvHandle();
				commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
			}
			int outputWidth = width;
			int outputHeight = height;
			ResolveOutputExtent(route, outputWidth, outputHeight);
			Dx12CommandContext::ApplyViewportAndScissor(commandList, outputWidth, outputHeight);
			UpdateInputDescriptors(sourceDevice, frameIndex, passIndex);
			commandList->SetGraphicsRootSignature(pipelines.GetRootSignature());
			commandList->SetPipelineState(pipelines.TryGetPipelineState(passIndex));
			commandList->SetGraphicsRootConstantBufferView(0, frameDataAddress);
			commandList->SetGraphicsRootConstantBufferView(
				1, parameterDataBuffer.GetGpuAddress() + parameterOffset);
			D3D12_GPU_DESCRIPTOR_HANDLE inputHandle = heap->GetGPUDescriptorHandleForHeapStart();
			inputHandle.ptr += passIndex * PostProcessInputLayout::maxTextureCount * inputDescriptorSize;
			commandList->SetGraphicsRootDescriptorTable(2, inputHandle);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(3, 1, 0, 0);
			if (outputTarget != nullptr)
				Dx12Barrier::Transition(commandList, enhancedCommandList, outputTarget->GetResource(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			AdvanceHistory(route);
		}
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		return {};
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
		inputDescriptorSize = 0;
	}
}
