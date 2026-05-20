#include "Dx11Viewer.h"

#include "Dx11Instance.h"
#include "Dx11DescriptorFactory.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <d3dcompiler.h>
#include <iostream>

namespace Chrivent {
	Dx11Viewer::Dx11Viewer() {
		info = std::make_unique<Dx11ViewerInfo>();
	}

	void Dx11Viewer::PrintShaderCompileError(const std::filesystem::path& file, const char* entry, const char* target, ID3DBlob* errorBlob) {
		std::cerr << "Failed to compile HLSL shader: " << file.string()
			<< " entry=" << entry << " target=" << target << '\n';
		if (errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr)
			std::cerr << static_cast<const char*>(errorBlob->GetBufferPointer()) << '\n';
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
		vp.Width = static_cast<float>(GetInfo().screenWidth);
		vp.Height = static_cast<float>(GetInfo().screenHeight);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		GetDx11Info().deviceResources.context->RSSetViewports(1, &vp);
	}

	bool Dx11Viewer::MakeVs(const std::filesystem::path& f, const char* entry,
		Microsoft::WRL::ComPtr<ID3D11VertexShader>& outVs, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob) const {
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		if (FAILED(D3DCompileFromFile(f.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entry, "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &outBlob, &errorBlob))) {
			PrintShaderCompileError(f, entry, "vs_5_0", errorBlob.Get());
			return false;
		}
		if (FAILED(GetDx11Info().deviceResources.device->CreateVertexShader(
			outBlob->GetBufferPointer(), outBlob->GetBufferSize(), nullptr, &outVs))) {
			std::cerr << "Failed to create DX11 vertex shader: " << f.string() << " entry=" << entry << '\n';
			return false;
		}
		return true;
	}

	bool Dx11Viewer::MakePs(const std::filesystem::path& f, const char* entry, Microsoft::WRL::ComPtr<ID3D11PixelShader>& outPs) const {
		Microsoft::WRL::ComPtr<ID3DBlob> blob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		if (FAILED(D3DCompileFromFile(f.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entry, "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errorBlob))) {
			PrintShaderCompileError(f, entry, "ps_5_0", errorBlob.Get());
			return false;
		}
		if (FAILED(GetDx11Info().deviceResources.device->CreatePixelShader(
			blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &outPs))) {
			std::cerr << "Failed to create DX11 pixel shader: " << f.string() << " entry=" << entry << '\n';
			return false;
		}
		return true;
	}

	bool Dx11Viewer::CreateShaders() {
		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, edgeVsBlob, gsVsBlob;
		if (!MakeVs(GetInfo().shaderDir / "mmd.hlsl", "VSMain", GetDx11Info().shaders.vs, vsBlob))
			return false;
		if (!MakeVs(GetInfo().shaderDir / "mmd_edge.hlsl", "VSMain", GetDx11Info().shaders.edgeVs, edgeVsBlob))
			return false;
		if (!MakeVs(GetInfo().shaderDir / "mmd_ground_shadow.hlsl", "VSMain", GetDx11Info().shaders.gsVs, gsVsBlob))
			return false;
		if (!MakePs(GetInfo().shaderDir / "mmd.hlsl", "PSMain", GetDx11Info().shaders.ps))
			return false;
		if (!MakePs(GetInfo().shaderDir / "mmd_edge.hlsl", "PSMain", GetDx11Info().shaders.edgePs))
			return false;
		if (!MakePs(GetInfo().shaderDir / "mmd_ground_shadow.hlsl", "PSMain", GetDx11Info().shaders.gsPs))
			return false;
		constexpr D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(GetDx11Info().deviceResources.device->CreateInputLayout(inputElementDesc, 3,
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			&GetDx11Info().shaders.inputLayout))) {
			std::cerr << "Failed to create DX11 input layout for model shader.\n";
			return false;
		}
		constexpr D3D11_INPUT_ELEMENT_DESC edgeInputElementDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(GetDx11Info().deviceResources.device->CreateInputLayout(edgeInputElementDesc, 2,
			edgeVsBlob->GetBufferPointer(), edgeVsBlob->GetBufferSize(),
			&GetDx11Info().shaders.edgeInputLayout))) {
			std::cerr << "Failed to create DX11 input layout for edge shader.\n";
			return false;
		}
		constexpr D3D11_INPUT_ELEMENT_DESC gsInputElementDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(GetDx11Info().deviceResources.device->CreateInputLayout(gsInputElementDesc, 1,
			gsVsBlob->GetBufferPointer(), gsVsBlob->GetBufferSize(),
			&GetDx11Info().shaders.gsInputLayout))) {
			std::cerr << "Failed to create DX11 input layout for ground shadow shader.\n";
			return false;
		}
		return true;
	}

	bool Dx11Viewer::CreateRenderTargets() {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		if (FAILED(GetDx11Info().deviceResources.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
			return false;
		if (FAILED(GetDx11Info().deviceResources.device->CreateRenderTargetView(backBuffer.Get(), nullptr, &GetDx11Info().renderTargets.renderTargetView)))
			return false;
		const auto d = Dx11DescriptorFactory::MakeTexture2DDesc(
			static_cast<UINT>(GetInfo().screenWidth), static_cast<UINT>(GetInfo().screenHeight),
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
		if (FAILED(GetDx11Info().deviceResources.device->CreateSamplerState(&clampLinear, &GetDx11Info().pipelineStates.cartoonTextureSampler)))
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
		const auto d = Dx11DescriptorFactory::MakeTexture2DDesc(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		if (FAILED(GetDx11Info().deviceResources.device->CreateTexture2D(&d, nullptr, &GetDx11Info().dummyTexture.texture)))
			return false;
		if (FAILED(GetDx11Info().deviceResources.device->CreateShaderResourceView(GetDx11Info().dummyTexture.texture.Get(), nullptr, &GetDx11Info().dummyTexture.textureView)))
			return false;
		return true;
	}
}


