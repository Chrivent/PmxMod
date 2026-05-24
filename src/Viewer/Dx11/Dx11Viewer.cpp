#include "Dx11Viewer.h"

#include "Dx11Instance.h"
#include "Helper/Dx11DescriptorFactory.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	Dx11Viewer::Dx11Viewer() {
		info = std::make_unique<Dx11ViewerInfo>();
	}

	void Dx11Viewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx11Viewer::Setup() {
		HWND__* hwnd = glfwGetWin32Window(GetInfo().window);
		constexpr D3D_FEATURE_LEVEL featureLevels = D3D_FEATURE_LEVEL_11_0;
		if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
			&featureLevels, 1, D3D11_SDK_VERSION, &GetDx11Info().deviceResources.device, nullptr, &GetDx11Info().deviceResources.context)))
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
		auto d = Dx11DescriptorFactory::MakeSwapChainDesc(hwnd, multiSampleCount, multiSampleQuality);
		if (FAILED(factory->CreateSwapChain(GetDx11Info().deviceResources.device.Get(), &d, &GetDx11Info().deviceResources.swapChain)))
			return false;
		if (!CreateRenderTargets())
			return false;
		InitDirs("shader_Dx11");
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

	std::unique_ptr<Instance> Dx11Viewer::CreateInstance() const {
		return std::make_unique<Dx11Instance>();
	}

	Dx11Texture Dx11Viewer::LoadTexture(const std::filesystem::path& texturePath) {
		return textureCache.Load(GetDx11Info().deviceResources.device.Get(), texturePath);
	}

	void Dx11Viewer::UpdateViewport() const {
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
		const auto d = Dx11DescriptorFactory::MakeTexture2DDesc(
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
		auto wrapLinear = Dx11DescriptorFactory::MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
		if (FAILED(GetDx11Info().deviceResources.device->CreateSamplerState(&wrapLinear, &GetDx11Info().pipelineStates.textureSampler)))
			return false;
		auto clampLinear = Dx11DescriptorFactory::MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
		if (FAILED(GetDx11Info().deviceResources.device->CreateSamplerState(&clampLinear, &GetDx11Info().pipelineStates.toonTextureSampler)))
			return false;
		auto blend = Dx11DescriptorFactory::MakeAlphaBlendDesc();
		if (FAILED(GetDx11Info().deviceResources.device->CreateBlendState(&blend, &GetDx11Info().pipelineStates.blendState)))
			return false;
		auto frontRsDesc = Dx11DescriptorFactory::MakeRasterizerDesc(D3D11_CULL_BACK, true);
		if (FAILED(GetDx11Info().deviceResources.device->CreateRasterizerState(&frontRsDesc, &GetDx11Info().pipelineStates.frontFaceRs)))
			return false;
		auto bothRsDesc = Dx11DescriptorFactory::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		if (FAILED(GetDx11Info().deviceResources.device->CreateRasterizerState(&bothRsDesc, &GetDx11Info().pipelineStates.bothFaceRs)))
			return false;
		auto edgeRsDesc = Dx11DescriptorFactory::MakeRasterizerDesc(D3D11_CULL_FRONT, true);
		edgeRsDesc.DepthClipEnable = FALSE;
		if (FAILED(GetDx11Info().deviceResources.device->CreateRasterizerState(&edgeRsDesc, &GetDx11Info().pipelineStates.edgeRs)))
			return false;
		auto gsRsDesc = Dx11DescriptorFactory::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		gsRsDesc.DepthClipEnable = FALSE;
		gsRsDesc.DepthBias = -1;
		gsRsDesc.SlopeScaledDepthBias = -1.0f;
		gsRsDesc.DepthBiasClamp = -1.0f;
		if (FAILED(GetDx11Info().deviceResources.device->CreateRasterizerState(&gsRsDesc, &GetDx11Info().pipelineStates.gsRs)))
			return false;
		auto gsDssDesc = Dx11DescriptorFactory::MakeGroundShadowDepthStencilDesc();
		if (FAILED(GetDx11Info().deviceResources.device->CreateDepthStencilState(&gsDssDesc, &GetDx11Info().pipelineStates.gsDss)))
			return false;
		auto defDssDesc  = Dx11DescriptorFactory::MakeDefaultDepthStencilDesc();
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
}


