#include "Viewer/Dx12/Dx12Viewer.h"

#include "Viewer/Dx12/Dx12Instance.h"
#include "Viewer/Dx12/Helper/Dx12Barrier.h"

#include <iostream>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	void Dx12Viewer::PrepareBackBufferForRendering(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer) const {
		Dx12Barrier::Transition(commandList, commandContext.GetEnhancedCommandList().Get(), backBuffer,
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Dx12Viewer::ClearRenderTargets(ID3D12GraphicsCommandList* commandList) const {
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = msaaColorBuffer.ResolveRtvHandle();
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthBuffer.ResolveDsvHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	}

	void Dx12Viewer::ApplyViewportAndScissor(ID3D12GraphicsCommandList* commandList) const {
		D3D12_VIEWPORT viewport{};
		viewport.Width = screenWidth;
		viewport.Height = screenHeight;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &viewport);
		D3D12_RECT scissorRect{};
		scissorRect.right = screenWidth;
		scissorRect.bottom = screenHeight;
		commandList->RSSetScissorRects(1, &scissorRect);
	}

	void Dx12Viewer::ResolveToBackBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer, ID3D12Resource* msaaColor) const {
		const D3D12_RESOURCE_STATES sourceState = device->msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_SOURCE
			: D3D12_RESOURCE_STATE_COPY_SOURCE;
		const D3D12_RESOURCE_STATES destinationState = device->msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_DEST
			: D3D12_RESOURCE_STATE_COPY_DEST;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			D3D12_RESOURCE_STATE_RENDER_TARGET, sourceState);
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, destinationState);
		if (device->msaaSampleCount > 1)
			commandList->ResolveSubresource(backBuffer, 0, msaaColor, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			commandList->CopyResource(backBuffer, msaaColor);
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			sourceState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			destinationState, D3D12_RESOURCE_STATE_PRESENT);
	}

	void Dx12Viewer::DrawPostProcess(ID3D12GraphicsCommandList* commandList,
		ID3D12Resource* backBuffer, ID3D12Resource* msaaColor) const {
		if (postProcessTargets.size() < 3) {
			ResolveToBackBuffer(commandList, backBuffer, msaaColor);
			return;
		}
		ID3D12Resource* sceneColor = postProcessTargets[0].ResolveResource();
		const D3D12_RESOURCE_STATES sourceState = device->msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_SOURCE
			: D3D12_RESOURCE_STATE_COPY_SOURCE;
		const D3D12_RESOURCE_STATES destinationState = device->msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_DEST
			: D3D12_RESOURCE_STATE_COPY_DEST;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			D3D12_RESOURCE_STATE_RENDER_TARGET, sourceState);
		Dx12Barrier::Transition(commandList, enhancedCommandList, sceneColor,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, destinationState);
		if (device->msaaSampleCount > 1)
			commandList->ResolveSubresource(sceneColor, 0, msaaColor, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			commandList->CopyResource(sceneColor, msaaColor);
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			sourceState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Dx12Barrier::Transition(commandList, enhancedCommandList, sceneColor,
			destinationState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		const size_t passCount = pipeline.GetPostProcessPassCount();
		for (size_t passIndex = 0; passIndex < passCount; passIndex++) {
			const bool lastPass = passIndex + 1 == passCount;
			const size_t sourceIndex = passIndex == 0 ? 0 : (passIndex - 1) % 2 + 1;
			const size_t targetIndex = passIndex % 2 + 1;
			if (!lastPass) {
				ID3D12Resource* target = postProcessTargets[targetIndex].ResolveResource();
				Dx12Barrier::Transition(commandList, enhancedCommandList, target,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				const D3D12_CPU_DESCRIPTOR_HANDLE targetRtv = postProcessTargets[targetIndex].ResolveRtvHandle();
				commandList->OMSetRenderTargets(1, &targetRtv, FALSE, nullptr);
			} else {
				const D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv = swapChain.ResolveCurrentRtvHandle();
				commandList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
			}
			ID3D12DescriptorHeap* descriptorHeaps[] = { postProcessTargets[sourceIndex].ResolveDescriptorHeap() };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
			pipeline.BindPostProcess(commandList, passIndex, postProcessTargets[sourceIndex].ResolveGpuHandle());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(3, 1, 0, 0);
			if (!lastPass) {
				ID3D12Resource* target = postProcessTargets[targetIndex].ResolveResource();
				Dx12Barrier::Transition(commandList, enhancedCommandList, target,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
		}
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	}

	bool Dx12Viewer::CreatePostProcessDepthTarget() {
		postProcessDepth.Reset();
		postProcessDepthDsvHeap.Reset();
		if (!device || !device->device || screenWidth <= 0 || screenHeight <= 0)
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		if (FAILED(device->device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&postProcessDepthDsvHeap))))
			return false;
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = screenWidth;
		resourceDesc.Height = screenHeight;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clearValue.DepthStencil.Depth = 1.0f;
		if (FAILED(device->device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&postProcessDepth))))
			return false;
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device->device->CreateDepthStencilView(postProcessDepth.Get(), &dsvDesc, ResolvePostProcessDepthDsvHandle());
		for (const auto& target : postProcessTargets) {
			target.UpdateDepthShaderResourceView(*device, postProcessDepth.Get());
			target.UpdateFocusHistoryShaderResourceView(*device, postProcessDepth.Get());
		}
		return true;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12Viewer::ResolvePostProcessDepthDsvHandle() const {
		if (!postProcessDepthDsvHeap)
			return {};
		return postProcessDepthDsvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	Dx12Viewer::Dx12Viewer() {
		device = std::make_shared<Dx12Device>();
		dummyTexture = std::make_shared<Dx12Texture>();
	}

	Dx12Viewer::~Dx12Viewer() {
		commandContext.WaitForGpu(*device);
		pipeline.Reset();
		commandContext.Reset();
		depthBuffer.Reset();
		postProcessDepth.Reset();
		postProcessDepthDsvHeap.Reset();
		postProcessTargets.clear();
		msaaColorBuffer.Reset();
		swapChain.Reset();
		device->Shutdown();
	}

	void Dx12Viewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx12Viewer::Setup() {
		InitDirs("shaders/pmxmod-default/effects");
		if (!device->Initialize()) {
			std::cerr << "Failed to initialize DX12 device.\n";
			return false;
		}
		capabilities = device->capabilities;
		if (!commandContext.Initialize(*device)) {
			std::cerr << "Failed to initialize DX12 command context.\n";
			return false;
		}
		commandList = commandContext.GetCommandList();
		HWND__* hwnd = glfwGetWin32Window(window);
		if (!swapChain.Initialize(*device, hwnd, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 swap chain.\n";
			return false;
		}
		if (!msaaColorBuffer.Initialize(*device, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 MSAA color buffer.\n";
			return false;
		}
		if (!depthBuffer.Initialize(*device, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 depth buffer.\n";
			return false;
		}
		postProcessTargets.resize(3);
		for (auto& target : postProcessTargets) {
			if (!target.Initialize(*device, screenWidth, screenHeight)) {
				std::cerr << "Failed to initialize DX12 post-process target.\n";
				return false;
			}
		}
		if (!CreatePostProcessDepthTarget()) {
			std::cerr << "Failed to initialize DX12 post-process depth target.\n";
			return false;
		}
		if (!pipeline.Initialize(*device, shaderDir)) {
			std::cerr << "Failed to initialize DX12 pipeline.\n";
			return false;
		}
		*dummyTexture = textureCache.CreateWhiteTexture(*device);
		if (!dummyTexture->resource) {
			std::cerr << "Failed to initialize DX12 dummy texture.\n";
			return false;
		}
		return true;
	}

	bool Dx12Viewer::Resize() {
		commandContext.WaitForGpu(*device);
		if (!swapChain.Resize(*device, screenWidth, screenHeight))
			return false;
		if (!msaaColorBuffer.Initialize(*device, screenWidth, screenHeight))
			return false;
		if (!depthBuffer.Initialize(*device, screenWidth, screenHeight))
			return false;
		postProcessTargets.resize(3);
		for (auto& target : postProcessTargets) {
			if (!target.Initialize(*device, screenWidth, screenHeight))
				return false;
		}
		if (!CreatePostProcessDepthTarget())
			return false;
		return true;
	}

	void Dx12Viewer::BeginFrame() {
		frameReady = false;
		const UINT frameIndex = swapChain.GetFrameIndex();
		if (!commandContext.BeginFrame(*device, frameIndex))
			return;
		this->frameIndex = frameIndex;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		ID3D12Resource* backBuffer = swapChain.ResolveCurrentBackBuffer();
		const ID3D12Resource* msaaColor = msaaColorBuffer.ResolveResource();
		if (!commandList || !backBuffer || !msaaColor)
			return;
		PrepareBackBufferForRendering(commandList, backBuffer);
		ClearRenderTargets(commandList);
		ApplyViewportAndScissor(commandList);
		pipeline.BindModel(commandList, false);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		frameReady = true;
	}

	bool Dx12Viewer::EndFrame() {
		if (!frameReady)
			return false;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		ID3D12Resource* backBuffer = swapChain.ResolveCurrentBackBuffer();
		ID3D12Resource* msaaColor = msaaColorBuffer.ResolveResource();
		if (!commandList || !backBuffer || !msaaColor)
			return false;
		if (pipeline.HasPostProcessEffect())
			DrawPostProcess(commandList, backBuffer, msaaColor);
		else
			ResolveToBackBuffer(commandList, backBuffer, msaaColor);
		if (!commandContext.Execute(*device))
			return false;
		if (!swapChain.Present())
			return false;
		frameReady = false;
		return true;
	}

	bool Dx12Viewer::BeginPostProcessDepthPass() {
		if (!frameReady || !pipeline.HasPostProcessEffect() || !postProcessDepth)
			return false;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		if (commandList == nullptr)
			return false;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, postProcessDepth.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = ResolvePostProcessDepthDsvHandle();
		commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		ApplyViewportAndScissor(commandList);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		return true;
	}

	void Dx12Viewer::EndPostProcessDepthPass() {
		if (!frameReady || !postProcessDepth)
			return;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		if (commandList == nullptr)
			return;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, postProcessDepth.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void Dx12Viewer::WaitIdle() {
		commandContext.WaitForGpu(*device);
	}

	bool Dx12Viewer::LoadPostProcessEffects(const std::vector<const EffectDefinition*>& effects) {
		WaitIdle();
		return pipeline.LoadPostProcessEffects(*device, effects);
	}

	void Dx12Viewer::ClearPostProcessEffects() {
		WaitIdle();
		pipeline.ClearPostProcessEffects();
	}

	std::unique_ptr<Instance> Dx12Viewer::CreateInstance() const {
		return std::make_unique<Dx12Instance>();
	}

	Dx12Texture Dx12Viewer::LoadTexture(const std::filesystem::path& texturePath) {
		return textureCache.Load(*device, texturePath);
	}

	void Dx12Viewer::BindModelPipeline(const bool bothFace) const {
		if (!frameReady)
			return;
		pipeline.BindModel(commandContext.GetCommandList().Get(), bothFace);
	}

	void Dx12Viewer::BindDepthOnlyPipeline(const bool bothFace) const {
		if (!frameReady)
			return;
		pipeline.BindDepthOnly(commandContext.GetCommandList().Get(), bothFace);
	}

	void Dx12Viewer::BindEdgePipeline() const {
		if (!frameReady)
			return;
		pipeline.BindEdge(commandContext.GetCommandList().Get());
	}

	void Dx12Viewer::BindGroundShadowPipeline() const {
		if (!frameReady)
			return;
		pipeline.BindGroundShadow(commandContext.GetCommandList().Get());
	}
}
