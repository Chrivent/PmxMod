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
				&featureLevels, 1, D3D11_SDK_VERSION, &GetDx11Info().deviceResources.device, nullptr,
				&GetDx11Info().deviceResources.context)))
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

	void Dx11Viewer::UpdateViewport() {
		D3D11_VIEWPORT vp;
		vp.Width = GetInfo().screenWidth;
		vp.Height = GetInfo().screenHeight;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		GetDx11Info().deviceResources.context->RSSetViewports(1, &vp);
	}

	bool Dx11Viewer::CreateShaders() {
		ID3D11Device* device = GetDx11Info().deviceResources.device.Get();
		return GetDx11Info().shaders.model.Initialize(device, GetInfo().shaderDir / "model.hlsl")
			&& GetDx11Info().shaders.edge.Initialize(device, GetInfo().shaderDir / "edge.hlsl")
			&& GetDx11Info().shaders.groundShadow.Initialize(device, GetInfo().shaderDir / "ground_shadow.hlsl");
	}

	bool Dx11Viewer::CreateRenderTargets() {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		if (FAILED(GetDx11Info().deviceResources.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
			return false;
		if (FAILED(GetDx11Info().deviceResources.device->CreateRenderTargetView(backBuffer.Get(), nullptr, &GetDx11Info().renderTargets.renderTargetView)))
			return false;
		const auto d = Dx11DescBuilder::MakeTexture2DDesc(
			GetInfo().screenWidth, GetInfo().screenHeight,
			DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL,
			multiSampleCount, multiSampleQuality);
		if (FAILED(GetDx11Info().deviceResources.device->CreateTexture2D(&d, nullptr, &GetDx11Info().renderTargets.depthTex)))
			return false;
		if (FAILED(GetDx11Info().deviceResources.device->CreateDepthStencilView(GetDx11Info().renderTargets.depthTex.Get(), nullptr, &GetDx11Info().renderTargets.depthStencilView)))
			return false;
		return true;
	}

	bool Dx11Viewer::CreatePipelineStates() {
		auto wrapLinear = Dx11DescBuilder::MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
		if (FAILED(GetDx11Info().deviceResources.device->CreateSamplerState(&wrapLinear, &GetDx11Info().pipelineStates.textureSampler)))
			return false;
		auto clampLinear = Dx11DescBuilder::MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
		if (FAILED(GetDx11Info().deviceResources.device->CreateSamplerState(&clampLinear, &GetDx11Info().pipelineStates.toonTextureSampler)))
			return false;
		auto blend = Dx11DescBuilder::MakeAlphaBlendDesc();
		if (FAILED(GetDx11Info().deviceResources.device->CreateBlendState(&blend, &GetDx11Info().pipelineStates.blendState)))
			return false;
		auto frontRsDesc = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_BACK, true);
		if (FAILED(GetDx11Info().deviceResources.device->CreateRasterizerState(&frontRsDesc, &GetDx11Info().pipelineStates.frontFaceRs)))
			return false;
		auto bothRsDesc = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		if (FAILED(GetDx11Info().deviceResources.device->CreateRasterizerState(&bothRsDesc, &GetDx11Info().pipelineStates.bothFaceRs)))
			return false;
		auto edgeRsDesc = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_FRONT, true);
		if (FAILED(GetDx11Info().deviceResources.device->CreateRasterizerState(&edgeRsDesc, &GetDx11Info().pipelineStates.edgeRs)))
			return false;
		auto gsRsDesc = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		gsRsDesc.DepthBias = -1;
		gsRsDesc.SlopeScaledDepthBias = -1.0f;
		gsRsDesc.DepthBiasClamp = -1.0f;
		if (FAILED(GetDx11Info().deviceResources.device->CreateRasterizerState(&gsRsDesc, &GetDx11Info().pipelineStates.gsRs)))
			return false;
		auto gsDssDesc = Dx11DescBuilder::MakeGroundShadowDepthStencilDesc();
		if (FAILED(GetDx11Info().deviceResources.device->CreateDepthStencilState(&gsDssDesc, &GetDx11Info().pipelineStates.gsDss)))
			return false;
		auto defDssDesc  = Dx11DescBuilder::MakeDefaultDepthStencilDesc();
		if (FAILED(GetDx11Info().deviceResources.device->CreateDepthStencilState(&defDssDesc, &GetDx11Info().pipelineStates.defaultDss)))
			return false;
		return true;
	}

	bool Dx11Viewer::CreateDummyResources() {
		const Dx11Texture dummyTexture = textureCache.CreateWhiteTexture(GetDx11Info().deviceResources.device.Get());
		GetDx11Info().dummyTexture.texture = dummyTexture.texture;
		GetDx11Info().dummyTexture.textureView = dummyTexture.textureView;
		return GetDx11Info().dummyTexture.texture && GetDx11Info().dummyTexture.textureView;
	}

	Dx11Viewer::Dx11Viewer() {
		info = std::make_unique<Dx11ViewerInfo>();
	}

	void Dx11Viewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx11Viewer::Setup() {
		HWND__* hwnd = glfwGetWin32Window(GetInfo().window);
		if (!CreateDevice())
			return false;
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		if (FAILED(GetDx11Info().deviceResources.device.As(&dxgiDevice)))
			return false;
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		if (FAILED(dxgiDevice->GetAdapter(&adapter)))
			return false;
		Microsoft::WRL::ComPtr<IDXGIFactory> factory;
		if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory), &factory)))
			return false;
		multiSampleCount = 4;
		UINT quality = 0;
		if (FAILED(GetDx11Info().deviceResources.device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, multiSampleCount, &quality)) || quality == 0) {
			multiSampleCount = 1;
			quality = 0;
		}
		multiSampleQuality = quality > 0 ? quality - 1 : 0;
		auto d = Dx11DescBuilder::MakeSwapChainDesc(hwnd, multiSampleCount, multiSampleQuality);
		if (FAILED(factory->CreateSwapChain(GetDx11Info().deviceResources.device.Get(), &d, &GetDx11Info().deviceResources.swapChain)))
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
		GetDx11Info().renderTargets.renderTargetView.Reset();
		GetDx11Info().renderTargets.depthStencilView.Reset();
		GetDx11Info().renderTargets.depthTex.Reset();
		if (FAILED(GetDx11Info().deviceResources.swapChain->ResizeBuffers(0, GetInfo().screenWidth, GetInfo().screenHeight, DXGI_FORMAT_UNKNOWN, 0)))
			return false;
		if (!CreateRenderTargets())
			return false;
		UpdateViewport();
		return true;
	}

	void Dx11Viewer::BeginFrame() {
		GetDx11Info().deviceResources.context->ClearRenderTargetView(GetDx11Info().renderTargets.renderTargetView.Get(), clearColor);
		GetDx11Info().deviceResources.context->ClearDepthStencilView(GetDx11Info().renderTargets.depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		GetDx11Info().deviceResources.context->OMSetRenderTargets(1, GetDx11Info().renderTargets.renderTargetView.GetAddressOf(), GetDx11Info().renderTargets.depthStencilView.Get());
		GetDx11Info().deviceResources.context->OMSetBlendState(GetDx11Info().pipelineStates.blendState.Get(), nullptr, 0xffffffff);
	}

	bool Dx11Viewer::EndFrame() {
		if (FAILED(GetDx11Info().deviceResources.swapChain->Present(0, 0)))
			return false;
		return true;
	}

	void Dx11Viewer::WaitIdle() {
		const auto& deviceResources = GetDx11Info().deviceResources;
		if (!deviceResources.device || !deviceResources.context)
			return;
		D3D11_QUERY_DESC queryDesc{};
		queryDesc.Query = D3D11_QUERY_EVENT;
		Microsoft::WRL::ComPtr<ID3D11Query> query;
		if (FAILED(deviceResources.device->CreateQuery(&queryDesc, &query)))
			return;
		deviceResources.context->End(query.Get());
		deviceResources.context->Flush();
		while (deviceResources.context->GetData(query.Get(), nullptr, 0, 0) == S_FALSE)
			SwitchToThread();
	}

	std::unique_ptr<Instance> Dx11Viewer::CreateInstance() const {
		return std::make_unique<Dx11Instance>();
	}

	Dx11Texture Dx11Viewer::LoadTexture(const std::filesystem::path& texturePath) {
		return textureCache.Load(GetDx11Info().deviceResources.device.Get(), texturePath);
	}
}


