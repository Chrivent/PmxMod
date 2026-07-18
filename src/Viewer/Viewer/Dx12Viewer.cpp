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
		const auto deviceResult = device.Initialize(capabilities);
		if (!deviceResult)
			return std::unexpected(deviceResult.error());
		const auto commandContextResult = commandContext.Initialize(device);
		if (!commandContextResult)
			return std::unexpected(commandContextResult.error());
		HWND__* hwnd = glfwGetWin32Window(window);
		const auto swapChainResult = swapChain.Initialize(device, hwnd, screenWidth, screenHeight);
		if (!swapChainResult)
			return std::unexpected(swapChainResult.error());
		if (!msaaColorBuffer.Initialize(device, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"MSAA color buffer 초기화", "DirectX 12 color buffer를 만들지 못했습니다"));
		if (!depthBuffer.Initialize(device, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"depth buffer 초기화", "DirectX 12 depth buffer를 만들지 못했습니다"));
		const auto pipelineResult = pipeline.Initialize(device, shaderContract);
		if (!pipelineResult)
			return std::unexpected(pipelineResult.error());
		dummyTexture = textureCache.CreateWhiteTexture(device);
		if (!dummyTexture.resource)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"dummy texture 생성", "fallback texture를 만들지 못했습니다"));
		return {};
	}

	GraphicsResult<void> Dx12Viewer::ResizeCore() {
		const auto waitResult = WaitIdle();
		if (!waitResult)
			return std::unexpected(waitResult.error());
		const auto resizeResult = swapChain.Resize(device, screenWidth, screenHeight);
		if (!resizeResult)
			return std::unexpected(resizeResult.error());
		if (!msaaColorBuffer.Initialize(device, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"MSAA color buffer 크기 변경", "DirectX 12 color buffer를 다시 만들지 못했습니다"));
		if (!depthBuffer.Initialize(device, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"depth buffer 크기 변경", "DirectX 12 depth buffer를 다시 만들지 못했습니다"));
		if (postProcess.HasEffects()) {
			if (!postProcess.InitializeTargets(device, screenWidth, screenHeight))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
					"후처리 target 크기 변경", "DirectX 12 후처리 target을 다시 만들지 못했습니다"));
		} else
			postProcess.ResetResources();
		return {};
	}

	GraphicsResult<FrameBeginState> Dx12Viewer::BeginFrameCore() {
		drawContext.EndFrame();
		const UINT frameIndex = swapChain.GetFrameIndex();
		const auto beginResult = commandContext.BeginFrame(device, frameIndex);
		if (!beginResult)
			return std::unexpected(beginResult.error());
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		ID3D12Resource* backBuffer = swapChain.GetCurrentBackBuffer();
		const ID3D12Resource* msaaColor = msaaColorBuffer.GetResource();
		if (!commandList || !backBuffer || !msaaColor)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"프레임 시작", "필요한 DirectX 12 프레임 리소스를 사용할 수 없습니다"));
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
				"프레임 종료", "DirectX 12 draw context가 준비되지 않았습니다"));
		drawContext.EndFrame();
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		ID3D12Resource* backBuffer = swapChain.GetCurrentBackBuffer();
		const ID3D12Resource* msaaColor = msaaColorBuffer.GetResource();
		if (!commandList || !backBuffer || !msaaColor)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"프레임 종료", "필요한 DirectX 12 프레임 리소스를 사용할 수 없습니다"));
		bool frameRecorded;
		if (postProcess.HasEffects())
			frameRecorded = postProcess.Draw(commandList, backBuffer, msaaColorBuffer,
				device, commandContext, swapChain, screenWidth, screenHeight, GetPostProcessFrameData());
		else
			frameRecorded = msaaColorBuffer.ResolveToBackBuffer(commandList,
				commandContext.GetEnhancedCommandList().Get(), backBuffer);
		if (!frameRecorded)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"프레임 기록", "DirectX 12 출력 패스를 기록하지 못했습니다"));
		const auto executeResult = commandContext.Execute(device);
		if (!executeResult)
			return std::unexpected(executeResult.error());
		const auto presentResult = swapChain.Present();
		if (!presentResult)
			return std::unexpected(presentResult.error());
		return FrameEndState::Presented;
	}

	GraphicsResult<void> Dx12Viewer::BeginPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 장면 입력 패스 시작", "DirectX 12 draw context가 준비되지 않았습니다"));
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		if (!postProcess.BeginSceneInputPass(commandList, commandContext, screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"후처리 장면 입력 패스 시작", "DirectX 12 장면 입력 패스를 시작하지 못했습니다"));
		return {};
	}

	GraphicsResult<void> Dx12Viewer::EndPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 장면 입력 패스 종료", "DirectX 12 draw context가 준비되지 않았습니다"));
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		if (!postProcess.EndSceneInputPass(commandList, commandContext))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"후처리 장면 입력 패스 종료", "DirectX 12 장면 입력 패스를 끝내지 못했습니다"));
		return {};
	}

	GraphicsResult<void> Dx12Viewer::WaitIdle() {
		if (!device.GetDevice())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"GPU 대기", "DirectX 12 device를 사용할 수 없습니다"));
		return commandContext.WaitForGpu(device);
	}

	GraphicsResult<void> Dx12Viewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (!device.GetDevice())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 효과 구성", "DirectX 12 device를 사용할 수 없습니다"));
		return postProcess.Configure(device, screenWidth, screenHeight, effects);
	}

	std::unique_ptr<Instance> Dx12Viewer::CreateInstanceCore() {
		return std::make_unique<Dx12Instance>(
			device, uploadContext, textureCache, dummyTexture, drawContext);
	}

}
