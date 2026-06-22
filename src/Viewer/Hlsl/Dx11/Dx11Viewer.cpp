#include "Dx11Viewer.h"

#include "Dx11Instance.h"
#include "Helper/Dx11DescBuilder.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>

namespace Chrivent {
	bool Dx11Viewer::CreateDevice() {
		Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
			return false;
		constexpr D3D_FEATURE_LEVEL featureLevels = D3D_FEATURE_LEVEL_11_0;
		for (UINT index = 0; ; index++) {
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			if (FAILED(factory->EnumAdapterByGpuPreference(
				index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))))
				break;
			DXGI_ADAPTER_DESC1 description{};
			if (FAILED(adapter->GetDesc1(&description)) || (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
				continue;
			if (FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
				&featureLevels, 1, D3D11_SDK_VERSION, &deviceResources.device, nullptr,
				&deviceResources.context)))
				continue;
			PrintGpuInfo(description);
			return true;
		}
		return false;
	}

	void Dx11Viewer::PrintGpuInfo(const DXGI_ADAPTER_DESC1& description) {
		std::wcout << L"dx11_gpu=" << description.Description << L'\n';
		std::wcout << L"dx11_gpu_type=" << (description.DedicatedVideoMemory > 0 ? L"discrete" : L"integrated") << L'\n';
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
		return shaders.model.Initialize(device, shaderDir / "model.hlsl")
			&& shaders.edge.Initialize(device, shaderDir / "edge.hlsl")
			&& shaders.groundShadow.Initialize(device, shaderDir / "ground_shadow.hlsl");
	}

	bool Dx11Viewer::CreateRenderTargets() {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		if (FAILED(deviceResources.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
			return false;
		if (FAILED(deviceResources.device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargets.renderTargetView)))
			return false;
		const auto d = Dx11DescBuilder::MakeTexture2DDesc(
			screenWidth, screenHeight,
			DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL,
			multiSampleCount, multiSampleQuality);
		if (FAILED(deviceResources.device->CreateTexture2D(&d, nullptr, &renderTargets.depthTex)))
			return false;
		if (FAILED(deviceResources.device->CreateDepthStencilView(renderTargets.depthTex.Get(), nullptr, &renderTargets.depthStencilView)))
			return false;
		return true;
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

	void Dx11Viewer::ConfigureGlfwHints() {
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
		auto d = Dx11DescBuilder::MakeSwapChainDesc(hwnd, multiSampleCount, multiSampleQuality);
		if (FAILED(factory->CreateSwapChain(deviceResources.device.Get(), &d, &deviceResources.swapChain)))
			return false;
		if (!CreateRenderTargets())
			return false;
		InitDirs("shader_hlsl");
		if (!CreateShaders())
			return false;
		if (!CreatePipelineStates())
			return false;
		if (!CreateDummyResources())
			return false;
		UpdateViewport();
		return true;
	}

	bool Dx11Viewer::Resize() {
		renderTargets.renderTargetView.Reset();
		renderTargets.depthStencilView.Reset();
		renderTargets.depthTex.Reset();
		if (FAILED(deviceResources.swapChain->ResizeBuffers(0, screenWidth, screenHeight, DXGI_FORMAT_UNKNOWN, 0)))
			return false;
		if (!CreateRenderTargets())
			return false;
		UpdateViewport();
		return true;
	}

	void Dx11Viewer::BeginFrame() {
		deviceResources.context->ClearRenderTargetView(renderTargets.renderTargetView.Get(), clearColor);
		deviceResources.context->ClearDepthStencilView(renderTargets.depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		deviceResources.context->OMSetRenderTargets(1, renderTargets.renderTargetView.GetAddressOf(), renderTargets.depthStencilView.Get());
		deviceResources.context->OMSetBlendState(pipelineStates.blendState.Get(), nullptr, 0xffffffff);
	}

	bool Dx11Viewer::EndFrame() {
		if (FAILED(deviceResources.swapChain->Present(0, 0)))
			return false;
		return true;
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

	std::unique_ptr<Instance> Dx11Viewer::CreateInstance() const {
		return std::make_unique<Dx11Instance>();
	}
}
