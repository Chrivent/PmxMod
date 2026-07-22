#include "Viewer/PostProcess/Dx12PostProcess.h"

#include "Viewer/Buffer/BufferSize.h"
#include "Viewer/Command/Dx12CommandContext.h"
#include "Viewer/Synchronization/Dx12Barrier.h"
#include "Viewer/SwapChain/Dx12SwapChain.h"
#include "Viewer/PostProcess/PostProcessFrameData.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"

#include <limits>

namespace Chrivent {
	GraphicsError::Result<void> Dx12PostProcess::CreateEffectResources(const Dx12Device& sourceDevice) {
		ResetEffectResources();
		const auto plans = GetResourcePlans();
		resources.resize(plans.size());
		for (size_t resourceIndex = 0; resourceIndex < plans.size(); resourceIndex++) {
			const ResourcePlan& plan = plans[resourceIndex];
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

	GraphicsError::Result<void> Dx12PostProcess::CreateInputDescriptorHeaps(const Dx12Device& sourceDevice) {
		inputDescriptorHeaps.clear();
		inputDescriptorSize = 0;
		if (!sourceDevice.GetDevice()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 input descriptor heap 생성",
				"DirectX 12 device를 사용할 수 없습니다"));
		}
		const size_t passCount = GetPassRoutes().size();
		if (passCount == 0)
			return {};
		if (passCount > std::numeric_limits<UINT>::max() / PostProcessInputLayout::maxTextureCount) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
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
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 input descriptor heap 생성",
					"DirectX 12 후처리 input descriptor heap을 만들지 못했습니다", result, true));
			}
		}
		inputDescriptorSize = sourceDevice.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		return {};
	}

	GraphicsError::Result<void> Dx12PostProcess::CreateParameterDataBuffers(const Dx12Device& sourceDevice) {
		for (auto& buffer : parameterDataBuffers)
			buffer.Reset();
		parameterDataStride = 0;
		if (!sourceDevice.GetDevice()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 parameter buffer 생성",
				"DirectX 12 device를 사용할 수 없습니다"));
		}
		const size_t passCount = GetPassRoutes().size();
		if (passCount == 0)
			return {};
		size_t bufferSize = 0;
		if (!Dx12Buffer::TryAlignConstantBufferSize(sizeof(ParameterData), parameterDataStride)
			|| !BufferSize::TryMultiply(parameterDataStride, passCount, bufferSize)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ContractViolation, "후처리 parameter buffer 크기 계산",
				"DirectX 12 후처리 패스 수가 parameter buffer 크기 한도를 넘습니다"));
		}
		for (auto& buffer : parameterDataBuffers) {
			const auto result = buffer.InitializeUpload(sourceDevice, bufferSize);
			if (!result)
				return std::unexpected(result.error());
		}
		return {};
	}

	ID3D12Resource* Dx12PostProcess::ResolveInputResource(
		const PassInputRoute& input, DXGI_FORMAT& format) const {
		if (input.kind == InputKind::SceneColor) {
			format = sceneColor.GetFormat();
			return sceneColor.GetResource();
		}
		if (input.kind == InputKind::SceneDepth) {
			format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			return depthTarget.GetResource();
		}
		if (input.kind == InputKind::SceneVelocity) {
			format = sceneVelocity.GetFormat();
			return sceneVelocity.GetResource();
		}
		if (input.resourceIndex >= resources.size()) {
			format = DXGI_FORMAT_UNKNOWN;
			return nullptr;
		}
		const auto& [targets] = resources[input.resourceIndex];
		const size_t index = ResolveResourceReadIndex(input.resourceIndex);
		format = targets[index].GetFormat();
		return targets[index].GetResource();
	}

	GraphicsError::Result<void> Dx12PostProcess::UpdateInputDescriptors(
		const Dx12Device& sourceDevice, const size_t frameIndex, const size_t passIndex) {
		ID3D12DescriptorHeap* heap = ResolveInputDescriptorHeap(frameIndex);
		if (!sourceDevice.GetDevice() || heap == nullptr || passIndex >= GetPassRoutes().size()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 입력 descriptor 갱신",
				"DirectX 12 device, descriptor heap 또는 pass index가 올바르지 않습니다"));
		}
		ID3D12Device* device = sourceDevice.GetDevice();
		D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += passIndex * PostProcessInputLayout::maxTextureCount * inputDescriptorSize;
		const PassInputRoute* slots[PostProcessInputLayout::maxTextureCount]{};
		for (const auto& input : GetPassRoutes()[passIndex].inputs)
			slots[input.slot] = &input;
		for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++) {
			DXGI_FORMAT colFormat = sceneColor.GetFormat();
			ID3D12Resource* colResource = sceneColor.GetResource();
			if (slots[slot] != nullptr)
				colResource = ResolveInputResource(*slots[slot], colFormat);
			if (colResource == nullptr || colFormat == DXGI_FORMAT_UNKNOWN) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
					GraphicsErrorCode::ContractViolation, "후처리 입력 texture 조회",
					"DirectX 12 후처리 pass의 입력 texture를 찾지 못했습니다"));
			}
			const size_t stateIndex =
				(frameIndex % FrameBuffering::dx12BufferCount * GetPassRoutes().size() + passIndex)
				* PostProcessInputLayout::maxTextureCount + slot;
			if (stateIndex >= inputDescriptorStates.size()) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
					GraphicsErrorCode::InvalidState, "후처리 입력 descriptor 갱신",
					"DirectX 12 후처리 descriptor 상태 색인이 올바르지 않습니다"));
			}
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
		return {};
	}

	void Dx12PostProcess::InitializeHistories(ID3D12GraphicsCommandList* commandList,
		const Dx12CommandContext& commandContext) {
		if (commandList == nullptr)
			return;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.TryGetEnhancedCommandList();
		constexpr float clearColor[4]{};
		const auto plans = GetResourcePlans();
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

	GraphicsError::Result<void> Dx12PostProcess::CreatePipelines(const Dx12Device& sourceDevice) {
		const auto passes = GetShaderPrograms();
		const auto routes = GetPassRoutes();
		std::vector<DXGI_FORMAT> formats;
		formats.reserve(passes.size());
		for (size_t index = 0; index < passes.size(); index++) {
			DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
			if (routes[index].outputKind == OutputKind::Resource) {
				const Dx12PostProcessTarget* target = ResolveOutputTarget(routes[index]);
				if (target == nullptr) {
					return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
						GraphicsErrorCode::ContractViolation, "후처리 pipeline 생성",
						"후처리 패스의 DirectX 12 출력 target을 찾지 못했습니다"));
				}
				format = target->GetFormat();
			}
			formats.emplace_back(format);
		}
		return pipelines.Initialize(sourceDevice, passes, formats);
	}

	ID3D12DescriptorHeap* Dx12PostProcess::ResolveInputDescriptorHeap(const size_t frameIndex) const {
		const size_t index = frameIndex % FrameBuffering::dx12BufferCount;
		return index < inputDescriptorHeaps.size() ? inputDescriptorHeaps[index].Get() : nullptr;
	}

	Dx12PostProcessTarget* Dx12PostProcess::ResolveOutputTarget(const PassRoute& route) {
		if (route.outputKind != OutputKind::Resource || route.outputResourceIndex >= resources.size())
			return nullptr;
		auto& [targets] = resources[route.outputResourceIndex];
		return &targets[ResolveResourceWriteIndex(route.outputResourceIndex)];
	}

	void Dx12PostProcess::ResetEffectResources() {
		inputDescriptorHeaps.clear();
		inputDescriptorStates.clear();
		inputDescriptorSize = 0;
		parameterDataStride = 0;
		resources.clear();
		for (auto& buffer : parameterDataBuffers)
			buffer.Reset();
	}

	GraphicsError::Result<void> Dx12PostProcess::InitializeTargets(
		const Dx12Device& sourceDevice, const int width, const int height) {
		if (!sourceDevice.GetDevice() || width <= 0 || height <= 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "후처리 target 생성",
				"DirectX 12 device 또는 후처리 target 크기가 올바르지 않습니다"));
		}
		targetWidth = width;
		targetHeight = height;
		sceneColor.Reset();
		sceneVelocity.Reset();
		ResetEffectResources();
		depthTarget.Reset();
		for (auto& buffer : frameDataBuffers)
			buffer.Reset();
		auto result = sceneColor.Initialize(sourceDevice, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
		if (result && RequiresVelocity())
			result = sceneVelocity.Initialize(sourceDevice, width, height, DXGI_FORMAT_R16G16_FLOAT);
		if (result && (RequiresDepth() || RequiresVelocity()))
			result = depthTarget.Initialize(sourceDevice, width, height);
		if (result)
			result = CreateEffectResources(sourceDevice);
		if (result)
			result = CreateInputDescriptorHeaps(sourceDevice);
		if (result)
			result = CreateParameterDataBuffers(sourceDevice);
		if (!result)
			return std::unexpected(result.error());
		size_t frameDataSize = 0;
		if (!Dx12Buffer::TryAlignConstantBufferSize(sizeof(PostProcessFrameData), frameDataSize)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ContractViolation, "후처리 frame buffer 크기 계산",
				"DirectX 12 후처리 frame buffer 크기가 한도를 넘습니다"));
		}
		for (auto& buffer : frameDataBuffers) {
			const auto bufferResult = buffer.InitializeUpload(sourceDevice, frameDataSize);
			if (!bufferResult)
				return std::unexpected(bufferResult.error());
		}
		return {};
	}

	void Dx12PostProcess::SwapResources(Dx12PostProcess& other) noexcept {
		std::swap(sceneColor, other.sceneColor);
		std::swap(sceneVelocity, other.sceneVelocity);
		resources.swap(other.resources);
		inputDescriptorHeaps.swap(other.inputDescriptorHeaps);
		inputDescriptorStates.swap(other.inputDescriptorStates);
		depthTarget.Swap(other.depthTarget);
		pipelines.Swap(other.pipelines);
		for (size_t index = 0; index < FrameBuffering::dx12BufferCount; index++) {
			frameDataBuffers[index].Swap(other.frameDataBuffers[index]);
			parameterDataBuffers[index].Swap(other.parameterDataBuffers[index]);
		}
		std::swap(targetWidth, other.targetWidth);
		std::swap(targetHeight, other.targetHeight);
		std::swap(inputDescriptorSize, other.inputDescriptorSize);
		std::swap(parameterDataStride, other.parameterDataStride);
	}

	GraphicsError::Result<void> Dx12PostProcess::Configure(const Dx12Device& sourceDevice,
		const int width, const int height, PreparedPostProcessEffects preparedEffects) {
		Dx12PostProcess candidate;
		candidate.AdoptPreparedEffects(std::move(preparedEffects));
		if (candidate.HasEffects()) {
			const auto targetResult = candidate.InitializeTargets(sourceDevice, width, height);
			if (!targetResult)
				return std::unexpected(targetResult.error());
			const auto pipelineResult = candidate.CreatePipelines(sourceDevice);
			if (!pipelineResult)
				return std::unexpected(pipelineResult.error());
		}
		SwapExecutionPlan(candidate);
		SwapResources(candidate);
		return {};
	}

	GraphicsError::Result<void> Dx12PostProcess::BeginSceneInputPass(ID3D12GraphicsCommandList* commandList,
		const Dx12CommandContext& commandContext, const int width, const int height) const {
		if ((!RequiresDepth() && !RequiresVelocity())
			|| depthTarget.GetResource() == nullptr || commandList == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 시작",
				"DirectX 12 command list 또는 후처리 장면 입력 target이 준비되지 않았습니다"));
		}
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.TryGetEnhancedCommandList();
		Dx12Barrier::Transition(commandList, enhancedCommandList, depthTarget.GetResource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		if (RequiresVelocity())
			Dx12Barrier::Transition(commandList, enhancedCommandList, sceneVelocity.GetResource(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthTarget.GetDsvHandle();
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

	GraphicsError::Result<void> Dx12PostProcess::EndSceneInputPass(
		ID3D12GraphicsCommandList* commandList, const Dx12CommandContext& commandContext) const {
		if (depthTarget.GetResource() == nullptr || commandList == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 종료",
				"DirectX 12 command list 또는 후처리 depth target을 사용할 수 없습니다"));
		}
		Dx12Barrier::Transition(commandList, commandContext.TryGetEnhancedCommandList(),
			depthTarget.GetResource(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		if (RequiresVelocity())
			Dx12Barrier::Transition(commandList, commandContext.TryGetEnhancedCommandList(),
				sceneVelocity.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		return {};
	}

	GraphicsError::Result<void> Dx12PostProcess::Draw(
		ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer,
		const Dx12MsaaColorBuffer& msaaColorBuffer, const Dx12Device& sourceDevice,
		const Dx12CommandContext& commandContext, const Dx12SwapChain& swapChain,
		const int width, const int height, const PostProcessFrameData& frameData) {
		ID3D12Resource* msaaColor = msaaColorBuffer.GetResource();
		if (!HasEffects() || commandList == nullptr || !sceneColor.GetResource()
			|| backBuffer == nullptr || msaaColor == nullptr
			|| !IsPassCountCompatible(pipelines.GetCount())) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 효과 draw",
				"DirectX 12 후처리 리소스 또는 실행 계획이 준비되지 않았습니다"));
		}
		const size_t frameIndex = swapChain.GetFrameIndex() % FrameBuffering::dx12BufferCount;
		const Dx12Buffer& frameDataBuffer = frameDataBuffers[frameIndex];
		const Dx12Buffer& parameterDataBuffer = parameterDataBuffers[frameIndex];
		if (parameterDataStride == 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 parameter stride 조회",
				"DirectX 12 후처리 parameter stride가 준비되지 않았습니다"));
		}
		if (!frameDataBuffer.Write(frameData) || !parameterDataBuffer.IsInitialized()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::CommandRecordingFailed, "후처리 frame data 기록",
				"DirectX 12 후처리 frame 또는 parameter buffer를 기록하지 못했습니다"));
		}
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.TryGetEnhancedCommandList();
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
		const auto routes = GetPassRoutes();
		ID3D12DescriptorHeap* heap = ResolveInputDescriptorHeap(frameIndex);
		if (heap == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidState, "후처리 descriptor binding",
				"DirectX 12 후처리 입력 descriptor heap을 사용할 수 없습니다"));
		}
		commandList->SetDescriptorHeaps(1, &heap);
		const D3D12_GPU_VIRTUAL_ADDRESS frameDataAddress = frameDataBuffer.GetGpuAddress();
		for (size_t passIndex = 0; passIndex < routes.size(); passIndex++) {
			const PassRoute& route = routes[passIndex];
			const size_t parameterOffset = passIndex * parameterDataStride;
			if (!parameterDataBuffer.Write(GetParameterData(route), parameterOffset)) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
					GraphicsErrorCode::CommandRecordingFailed, "후처리 parameter 기록",
					"DirectX 12 후처리 pass parameter를 기록하지 못했습니다"));
			}
			const Dx12PostProcessTarget* outputTarget = ResolveOutputTarget(route);
			if (route.outputKind == OutputKind::Resource && outputTarget == nullptr) {
				DiscardHistoryFrame();
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
					GraphicsErrorCode::ContractViolation, "후처리 출력 target 조회",
					"DirectX 12 후처리 pass의 출력 target을 찾지 못했습니다"));
			}
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
			const auto descriptorResult = UpdateInputDescriptors(sourceDevice, frameIndex, passIndex);
			if (!descriptorResult) {
				DiscardHistoryFrame();
				return std::unexpected(descriptorResult.error());
			}
			if (pipelines.GetRootSignature() == nullptr
				|| pipelines.TryGetPipelineState(passIndex) == nullptr) {
				DiscardHistoryFrame();
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
					GraphicsErrorCode::InvalidState, "후처리 pipeline binding",
					"DirectX 12 후처리 root signature 또는 pipeline state를 사용할 수 없습니다"));
			}
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
		depthTarget.Reset();
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
