#include "Viewer/Viewer/Dx12Viewer.h"

#include "Viewer/Instance/Dx12Instance.h"
#include "Viewer/Synchronization/Dx12Barrier.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	void Dx12Viewer::PrepareBackBufferForRendering(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer) const {
		Dx12Barrier::Transition(commandList, commandContext.GetEnhancedCommandList().Get(), backBuffer,
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Dx12Viewer::ClearRenderTargets(ID3D12GraphicsCommandList* commandList) const {
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = msaaColorBuffer.GetRtvHandle();
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthBuffer.GetDsvHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	}

	GraphicsResult<void> Dx12Viewer::SetupCore(const SceneShaderRuntimeContract& shaderContract) {
		BindPostProcess(postProcess);
		if (!device.Initialize())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InitializationFailed,
				"initialize device", "the DirectX 12 device could not be created"));
		capabilities = device.capabilities;
		if (!commandContext.Initialize(device))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InitializationFailed,
				"initialize command context", "the DirectX 12 command context could not be created"));
		HWND__* hwnd = glfwGetWin32Window(window);
		if (!swapChain.Initialize(device, hwnd, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize swap chain", "the DirectX 12 swap chain could not be created"));
		if (!msaaColorBuffer.Initialize(device, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize MSAA color buffer", "the DirectX 12 color buffer could not be created"));
		if (!depthBuffer.Initialize(device, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize depth buffer", "the DirectX 12 depth buffer could not be created"));
		if (postProcess.HasEffects() && !postProcess.InitializeTargets(device, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize post-process targets", "the DirectX 12 post-process targets could not be created"));
		if (!pipeline.Initialize(device, shaderContract.builtIn,
			shaderContract.sceneInput.depth, shaderContract.sceneInput.velocity)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize rendering pipeline", "the DirectX 12 pipeline could not be created"));
		}
		dummyTexture = textureCache.CreateWhiteTexture(device);
		if (!dummyTexture.resource)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"create dummy texture", "the fallback texture could not be created"));
		return {};
	}

	GraphicsResult<void> Dx12Viewer::ResizeCore() {
		const auto waitResult = WaitIdle();
		if (!waitResult)
			return std::unexpected(waitResult.error());
		if (!swapChain.Resize(device, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"resize swap chain", "the DirectX 12 swap chain could not be resized"));
		if (!msaaColorBuffer.Initialize(device, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"resize MSAA color buffer", "the DirectX 12 color buffer could not be recreated"));
		if (!depthBuffer.Initialize(device, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"resize depth buffer", "the DirectX 12 depth buffer could not be recreated"));
		if (postProcess.HasEffects()) {
			if (!postProcess.InitializeTargets(device, screenWidth, screenHeight))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
					"resize post-process targets", "the DirectX 12 post-process targets could not be recreated"));
		} else
			postProcess.ResetResources();
		return {};
	}

	GraphicsResult<FrameBeginState> Dx12Viewer::BeginFrameCore() {
		drawContext.EndFrame();
		const UINT frameIndex = swapChain.GetFrameIndex();
		if (!commandContext.BeginFrame(device, frameIndex))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"begin frame", "the DirectX 12 command context could not begin recording"));
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		ID3D12Resource* backBuffer = swapChain.GetCurrentBackBuffer();
		const ID3D12Resource* msaaColor = msaaColorBuffer.GetResource();
		if (!commandList || !backBuffer || !msaaColor)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"begin frame", "required DirectX 12 frame resources are unavailable"));
		PrepareBackBufferForRendering(commandList, backBuffer);
		ClearRenderTargets(commandList);
		Dx12CommandContext::ApplyViewportAndScissor(commandList, screenWidth, screenHeight);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		drawContext.BeginFrame(frameIndex);
		return FrameBeginState::Ready;
	}

	GraphicsResult<FrameEndState> Dx12Viewer::EndFrameCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"end frame", "the DirectX 12 draw context is not ready"));
		drawContext.EndFrame();
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		ID3D12Resource* backBuffer = swapChain.GetCurrentBackBuffer();
		const ID3D12Resource* msaaColor = msaaColorBuffer.GetResource();
		if (!commandList || !backBuffer || !msaaColor)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"end frame", "required DirectX 12 frame resources are unavailable"));
		bool frameRecorded;
		if (postProcess.HasEffects())
			frameRecorded = postProcess.Draw(commandList, backBuffer, msaaColorBuffer,
				device, commandContext, swapChain, screenWidth, screenHeight, GetPostProcessFrameData());
		else
			frameRecorded = msaaColorBuffer.ResolveToBackBuffer(commandList,
				commandContext.GetEnhancedCommandList().Get(), backBuffer);
		if (!frameRecorded)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"record frame", "the DirectX 12 output pass could not be recorded"));
		if (!commandContext.Execute(device))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandSubmissionFailed,
				"submit frame", "the DirectX 12 command list could not be submitted"));
		if (!swapChain.Present())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::PresentationFailed,
				"present swap chain", "the DirectX 12 frame could not be presented"));
		return FrameEndState::Presented;
	}

	GraphicsResult<void> Dx12Viewer::BeginPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"begin post-process scene input pass", "the DirectX 12 draw context is not ready"));
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		if (!postProcess.BeginSceneInputPass(commandList, commandContext, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"begin post-process scene input pass", "the DirectX 12 scene input pass could not begin"));
		return {};
	}

	GraphicsResult<void> Dx12Viewer::EndPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"end post-process scene input pass", "the DirectX 12 draw context is not ready"));
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		if (!postProcess.EndSceneInputPass(commandList, commandContext))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"end post-process scene input pass", "the DirectX 12 scene input pass could not end"));
		return {};
	}

	GraphicsResult<void> Dx12Viewer::WaitIdle() {
		if (!device.device)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"wait for GPU", "the DirectX 12 device is unavailable"));
		if (!commandContext.WaitForGpu(device))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"wait for GPU", "the DirectX 12 command queue did not finish"));
		return {};
	}

	GraphicsResult<void> Dx12Viewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (!device.device)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"configure post-process effects", "the DirectX 12 device is unavailable"));
		if (!postProcess.Configure(device, screenWidth, screenHeight, effects))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::EffectConfigurationFailed,
				"configure post-process effects", "the DirectX 12 effect chain could not be created"));
		return {};
	}

	std::unique_ptr<Instance> Dx12Viewer::CreateInstanceCore() {
		return std::make_unique<Dx12Instance>(
			*this, device, uploadContext, textureCache, dummyTexture, drawContext);
	}

}
