#include "Viewer/Viewer/Dx11Viewer.h"

#include "Viewer/Instance/Dx11Instance.h"
#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Util.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <iterator>

namespace Chrivent {
	bool Dx11Viewer::CreateDevice() {
		Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
			return false;
		constexpr D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};
		for (UINT index = 0; ; index++) {
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			if (FAILED(factory->EnumAdapterByGpuPreference(
				index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))))
				break;
			DXGI_ADAPTER_DESC1 description{};
			if (FAILED(adapter->GetDesc1(&description)) || (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
				continue;
			D3D_FEATURE_LEVEL selectedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
			HRESULT result = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
				featureLevels, std::size(featureLevels), D3D11_SDK_VERSION,
				&deviceResources.device, &selectedFeatureLevel, &deviceResources.context);
			if (result == E_INVALIDARG) {
				result = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
					&featureLevels[1], 1, D3D11_SDK_VERSION, &deviceResources.device,
					&selectedFeatureLevel, &deviceResources.context);
			}
			if (FAILED(result))
				continue;
			capabilities.apiName = "Direct3D 11";
			capabilities.apiVersion = selectedFeatureLevel == D3D_FEATURE_LEVEL_11_1
				? "Feature Level 11.1" : "Feature Level 11.0";
			capabilities.shaderVersion = "Shader Model 5.0";
			capabilities.gpuName = Util::WStringToUtf8(description.Description);
			capabilities.gpuType = description.DedicatedVideoMemory > 0 ? "discrete" : "integrated";
			capabilities.uniformBufferAlignment = 16;
			capabilities.maxTextureBindings = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
			capabilities.shaderModelMajor = 5;
			return true;
		}
		return false;
	}

	void Dx11Viewer::UpdateViewport() const {
		D3D11_VIEWPORT vp;
		vp.Width = screenWidth;
		vp.Height = screenHeight;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		deviceResources.context->RSSetViewports(1, &vp);
	}

	bool Dx11Viewer::CreateShaders() {
		ID3D11Device* device = deviceResources.device.Get();
		return shaders.model.Initialize(device, builtInShaderPasses.model)
			&& shaders.edge.Initialize(device, builtInShaderPasses.edge)
			&& shaders.groundShadow.Initialize(device, builtInShaderPasses.groundShadow)
			&& shaders.sceneDepth.Initialize(device, sceneInputShaderPasses.depth)
			&& shaders.sceneVelocity.Initialize(device, sceneInputShaderPasses.velocityInvertedY);
	}

	bool Dx11Viewer::CreateRenderTargets() {
		const auto& device = deviceResources.device;
		if (FAILED(deviceResources.swapChain->GetBuffer(0, IID_PPV_ARGS(renderTargets.backBuffer.GetAddressOf()))))
			return false;
		if (FAILED(device->CreateRenderTargetView(renderTargets.backBuffer.Get(), nullptr, &renderTargets.backBufferView)))
			return false;
		const auto sceneColorMsaaDesc = Dx11DescBuilder::MakeTexture2DDesc(screenWidth, screenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, multiSampleCount, multiSampleQuality);
		if (FAILED(device->CreateTexture2D(&sceneColorMsaaDesc, nullptr, &renderTargets.sceneColorMsaa)))
			return false;
		if (FAILED(device->CreateRenderTargetView(renderTargets.sceneColorMsaa.Get(), nullptr, &renderTargets.sceneColorMsaaView)))
			return false;
		const auto depthDesc = Dx11DescBuilder::MakeTexture2DDesc(screenWidth, screenHeight, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL, multiSampleCount, multiSampleQuality);
		if (FAILED(device->CreateTexture2D(&depthDesc, nullptr, &renderTargets.depthTex)))
			return false;
		if (FAILED(device->CreateDepthStencilView(renderTargets.depthTex.Get(), nullptr, &renderTargets.depthStencilView)))
			return false;
		return postProcess.InitializeTargets(
			deviceResources.device.Get(), deviceResources.context.Get(), screenWidth, screenHeight);
	}

	bool Dx11Viewer::CreatePipelineStates() {
		auto wrapLinear = Dx11DescBuilder::MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
		if (FAILED(deviceResources.device->CreateSamplerState(&wrapLinear, &pipelineStates.textureSampler)))
			return false;
		auto clampLinear = Dx11DescBuilder::MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
		if (FAILED(deviceResources.device->CreateSamplerState(&clampLinear, &pipelineStates.toonTextureSampler)))
			return false;
		auto blend = Dx11DescBuilder::MakeAlphaBlendDesc();
		if (FAILED(deviceResources.device->CreateBlendState(&blend, &pipelineStates.blendState)))
			return false;
		auto frontRsDesc = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_BACK, true);
		if (FAILED(deviceResources.device->CreateRasterizerState(&frontRsDesc, &pipelineStates.frontFaceRs)))
			return false;
		auto bothRsDesc = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		if (FAILED(deviceResources.device->CreateRasterizerState(&bothRsDesc, &pipelineStates.bothFaceRs)))
			return false;
		auto edgeRsDesc = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_FRONT, true);
		if (FAILED(deviceResources.device->CreateRasterizerState(&edgeRsDesc, &pipelineStates.edgeRs)))
			return false;
		auto gsRsDesc = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		gsRsDesc.DepthBias = -1;
		gsRsDesc.SlopeScaledDepthBias = -1.0f;
		gsRsDesc.DepthBiasClamp = -1.0f;
		if (FAILED(deviceResources.device->CreateRasterizerState(&gsRsDesc, &pipelineStates.gsRs)))
			return false;
		auto gsDssDesc = Dx11DescBuilder::MakeGroundShadowDepthStencilDesc();
		if (FAILED(deviceResources.device->CreateDepthStencilState(&gsDssDesc, &pipelineStates.gsDss)))
			return false;
		auto defDssDesc  = Dx11DescBuilder::MakeDefaultDepthStencilDesc();
		if (FAILED(deviceResources.device->CreateDepthStencilState(&defDssDesc, &pipelineStates.defaultDss)))
			return false;
		return true;
	}

	bool Dx11Viewer::CreateDummyResources() {
		const Dx11Texture texture = textureCache.CreateWhiteTexture(deviceResources.device.Get());
		dummyTexture.texture = texture.texture;
		dummyTexture.textureView = texture.textureView;
		return dummyTexture.texture && dummyTexture.textureView;
	}

	void Dx11Viewer::ConfigureWindowHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx11Viewer::Setup() {
		HWND__* hwnd = glfwGetWin32Window(window);
		if (!CreateDevice())
			return false;
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		if (FAILED(deviceResources.device.As(&dxgiDevice)))
			return false;
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		if (FAILED(dxgiDevice->GetAdapter(&adapter)))
			return false;
		Microsoft::WRL::ComPtr<IDXGIFactory> factory;
		if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory), &factory)))
			return false;
		multiSampleCount = 4;
		UINT quality = 0;
		if (FAILED(deviceResources.device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, multiSampleCount, &quality)) || quality == 0) {
			multiSampleCount = 1;
			quality = 0;
		}
		multiSampleQuality = quality > 0 ? quality - 1 : 0;
		capabilities.maxSampleCount = multiSampleCount;
		capabilities.Print();
		auto d = Dx11DescBuilder::MakeSwapChainDesc(hwnd, multiSampleCount, multiSampleQuality);
		if (FAILED(factory->CreateSwapChain(deviceResources.device.Get(), &d, &deviceResources.swapChain)))
			return false;
		if (!CreateRenderTargets())
			return false;
		if (!InitializeShaderResources() || !CreateShaders())
			return false;
		if (!CreatePipelineStates())
			return false;
		if (!CreateDummyResources())
			return false;
		UpdateViewport();
		return true;
	}

	bool Dx11Viewer::Resize() {
		deviceResources.context->OMSetRenderTargets(0, nullptr, nullptr);
		renderTargets.backBuffer.Reset();
		renderTargets.backBufferView.Reset();
		renderTargets.sceneColorMsaa.Reset();
		renderTargets.sceneColorMsaaView.Reset();
		renderTargets.depthStencilView.Reset();
		renderTargets.depthTex.Reset();
		postProcess.ResetTargets();
		if (FAILED(deviceResources.swapChain->ResizeBuffers(0, screenWidth, screenHeight, DXGI_FORMAT_UNKNOWN, 0)))
			return false;
		if (!CreateRenderTargets())
			return false;
		UpdateViewport();
		return true;
	}

	FrameBeginResult Dx11Viewer::BeginFrame() {
		deviceResources.context->ClearRenderTargetView(renderTargets.sceneColorMsaaView.Get(), clearColor);
		deviceResources.context->ClearDepthStencilView(renderTargets.depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		deviceResources.context->OMSetRenderTargets(1, renderTargets.sceneColorMsaaView.GetAddressOf(), renderTargets.depthStencilView.Get());
		deviceResources.context->OMSetBlendState(pipelineStates.blendState.Get(), nullptr, 0xffffffff);
		return FrameBeginResult::Ready;
	}

	FrameEndResult Dx11Viewer::EndFrame() {
		if (postProcess.HasEffects()) {
			postProcess.ResolveSceneColor(
				deviceResources.context.Get(), renderTargets.sceneColorMsaa.Get(), multiSampleCount);
			if (!postProcess.Draw(deviceResources.context.Get(), renderTargets.backBufferView.Get(),
				pipelineStates.bothFaceRs.Get(), pipelineStates.toonTextureSampler.Get(),
				screenWidth, screenHeight, postProcessFrameData))
				return FrameEndResult::Failed;
		} else {
			deviceResources.context->CopyResource(
				renderTargets.backBuffer.Get(), renderTargets.sceneColorMsaa.Get());
		}
		if (FAILED(deviceResources.swapChain->Present(0, 0)))
			return FrameEndResult::Failed;
		return FrameEndResult::Presented;
	}

	bool Dx11Viewer::BeginPostProcessDepthPass() {
		return postProcess.BeginDepthPass(
			deviceResources.context.Get(), pipelineStates.defaultDss.Get(), screenWidth, screenHeight);
	}

	void Dx11Viewer::EndPostProcessDepthPass() {
		postProcess.EndDepthPass(deviceResources.context.Get());
	}

	void Dx11Viewer::WaitIdle() {
		const auto& resources = deviceResources;
		if (!resources.device || !resources.context)
			return;
		D3D11_QUERY_DESC queryDesc{};
		queryDesc.Query = D3D11_QUERY_EVENT;
		Microsoft::WRL::ComPtr<ID3D11Query> query;
		if (FAILED(resources.device->CreateQuery(&queryDesc, &query)))
			return;
		resources.context->End(query.Get());
		resources.context->Flush();
		while (resources.context->GetData(query.Get(), nullptr, 0, 0) == S_FALSE)
			SwitchToThread();
	}

	bool Dx11Viewer::LoadPostProcessEffects(const std::vector<const EffectDefinition*>& effects) {
		return FinishPostProcessLoad(postProcess.Load(deviceResources.device.Get(), effects));
	}

	std::unique_ptr<Instance> Dx11Viewer::CreateInstance() {
		return std::make_unique<Dx11Instance>(*this);
	}
}
