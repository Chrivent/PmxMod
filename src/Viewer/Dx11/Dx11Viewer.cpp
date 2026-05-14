#include "Dx11Viewer.h"

#include "Dx11Instance.h"
#include "Dx11DescHelper.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <d3dcompiler.h>
#include <stb_image.h>

namespace Chrivent {
	using namespace Dx11DescHelper;
	
	void Dx11Viewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx11Viewer::Setup() {
		HWND__* hwnd = glfwGetWin32Window(window);
		constexpr D3D_FEATURE_LEVEL featureLevels = D3D_FEATURE_LEVEL_11_0;
		if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
			&featureLevels, 1, D3D11_SDK_VERSION, &device, nullptr, &context)))
			return false;
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		if (FAILED(device.As(&dxgiDevice)))
			return false;
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		if (FAILED(dxgiDevice->GetAdapter(&adapter)))
			return false;
		Microsoft::WRL::ComPtr<IDXGIFactory> factory;
		if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory), &factory)))
			return false;
		multiSampleCount = 4;
		UINT quality = 0;
		if (FAILED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, multiSampleCount, &quality)) || quality == 0) {
			multiSampleCount = 1;
			quality = 0;
		}
		multiSampleQuality = quality > 0 ? quality - 1 : 0;
		auto d = MakeSwapChainDesc(hwnd, multiSampleCount, multiSampleQuality);
		if (FAILED(factory->CreateSwapChain(device.Get(), &d, &swapChain)))
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
		renderTargetView.Reset();
		depthStencilView.Reset();
		depthTex.Reset();
		if (FAILED(swapChain->ResizeBuffers(0, screenWidth, screenHeight, DXGI_FORMAT_UNKNOWN, 0)))
			return false;
		if (!CreateRenderTargets())
			return false;
		UpdateViewport();
		return true;
	}

	void Dx11Viewer::BeginFrame() {
		context->ClearRenderTargetView(renderTargetView.Get(), clearColor);
		context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());
		context->OMSetBlendState(blendState.Get(), nullptr, 0xffffffff);
	}

	bool Dx11Viewer::EndFrame() {
		if (FAILED(swapChain->Present(0, 0)))
			return false;
		return true;
	}

	std::unique_ptr<Instance> Dx11Viewer::CreateInstance() const {
		return std::make_unique<Dx11Instance>();
	}

	Dx11Texture Dx11Viewer::LoadTexture(const std::filesystem::path& texturePath) {
		const auto it = textures.find(texturePath);
		if (it != textures.end())
			return it->second;
		int x = 0, y = 0, comp = 0;
		stbi_uc* image = LoadImageRgba(texturePath, x, y, comp);
		if (!image)
			return {};
		const bool textureHasAlpha = comp == 4;
		const auto d = MakeTexture2DDesc(
			static_cast<UINT>(x), static_cast<UINT>(y),
			DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = image;
		initData.SysMemPitch = 4 * x;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
		const HRESULT hr = device->CreateTexture2D(&d, &initData, &tex2D);
		stbi_image_free(image);
		if (FAILED(hr))
			return {};
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex2DRv;
		if (FAILED(device->CreateShaderResourceView(tex2D.Get(), nullptr, &tex2DRv)))
			return {};
		Dx11Texture tex;
		tex.texture = tex2D;
		tex.textureView = tex2DRv;
		tex.hasAlpha = textureHasAlpha;
		textures[texturePath] = tex;
		return textures[texturePath];
	}

	void Dx11Viewer::UpdateViewport() const {
		D3D11_VIEWPORT vp;
		vp.Width = static_cast<float>(screenWidth);
		vp.Height = static_cast<float>(screenHeight);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		context->RSSetViewports(1, &vp);
	}

	bool Dx11Viewer::MakeVs(const std::filesystem::path& f, const char* entry,
		Microsoft::WRL::ComPtr<ID3D11VertexShader>& outVs, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob) const {
		if (FAILED(D3DCompileFromFile(f.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entry, "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &outBlob, nullptr)))
			return false;
		if (FAILED(device->CreateVertexShader(
			outBlob->GetBufferPointer(), outBlob->GetBufferSize(), nullptr, &outVs)))
			return false;
		return true;
	}

	bool Dx11Viewer::MakePs(const std::filesystem::path& f, const char* entry, Microsoft::WRL::ComPtr<ID3D11PixelShader>& outPs) const {
		Microsoft::WRL::ComPtr<ID3DBlob> blob;
		if (FAILED(D3DCompileFromFile(f.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entry, "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, nullptr)))
			return false;
		if (FAILED(device->CreatePixelShader(
			blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &outPs)))
			return false;
		return true;
	}

	bool Dx11Viewer::CreateShaders() {
		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, edgeVsBlob, gsVsBlob;
		if (!MakeVs(shaderDir / "mmd.hlsl", "VSMain", vs, vsBlob))
			return false;
		if (!MakeVs(shaderDir / "mmd_edge.hlsl", "VSMain", edgeVs, edgeVsBlob))
			return false;
		if (!MakeVs(shaderDir / "mmd_ground_shadow.hlsl", "VSMain", gsVs, gsVsBlob))
			return false;
		if (!MakePs(shaderDir / "mmd.hlsl", "PSMain", ps))
			return false;
		if (!MakePs(shaderDir / "mmd_edge.hlsl", "PSMain", edgePs))
			return false;
		if (!MakePs(shaderDir / "mmd_ground_shadow.hlsl", "PSMain", gsPs))
			return false;
		constexpr D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(device->CreateInputLayout(inputElementDesc, 3,
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			&inputLayout)))
			return false;
		constexpr D3D11_INPUT_ELEMENT_DESC edgeInputElementDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(device->CreateInputLayout(edgeInputElementDesc, 2,
			edgeVsBlob->GetBufferPointer(), edgeVsBlob->GetBufferSize(),
			&edgeInputLayout)))
			return false;
		constexpr D3D11_INPUT_ELEMENT_DESC gsInputElementDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(device->CreateInputLayout(gsInputElementDesc, 1,
			gsVsBlob->GetBufferPointer(), gsVsBlob->GetBufferSize(),
			&gsInputLayout)))
			return false;
		return true;
	}

	bool Dx11Viewer::CreateRenderTargets() {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
			return false;
		if (FAILED(device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView)))
			return false;
		const auto d = MakeTexture2DDesc(
			static_cast<UINT>(screenWidth), static_cast<UINT>(screenHeight),
			DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL,
			multiSampleCount, multiSampleQuality);
		if (FAILED(device->CreateTexture2D(&d, nullptr, &depthTex)))
			return false;
		if (FAILED(device->CreateDepthStencilView(depthTex.Get(), nullptr, &depthStencilView)))
			return false;
		return true;
	}

	bool Dx11Viewer::CreatePipelineStates() {
		auto wrapLinear = MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
		if (FAILED(device->CreateSamplerState(&wrapLinear, &textureSampler)))
			return false;
		auto clampLinear = MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
		if (FAILED(device->CreateSamplerState(&clampLinear, &cartoonTextureSampler)))
			return false;
		auto blend = MakeAlphaBlendDesc();
		if (FAILED(device->CreateBlendState(&blend, &blendState)))
			return false;
		auto frontRsDesc = MakeRasterizerDesc(D3D11_CULL_BACK, true);
		if (FAILED(device->CreateRasterizerState(&frontRsDesc, &frontFaceRs)))
			return false;
		auto bothRsDesc = MakeRasterizerDesc(D3D11_CULL_NONE, true);
		if (FAILED(device->CreateRasterizerState(&bothRsDesc, &bothFaceRs)))
			return false;
		auto edgeRsDesc = MakeRasterizerDesc(D3D11_CULL_FRONT, true);
		edgeRsDesc.DepthClipEnable = FALSE;
		if (FAILED(device->CreateRasterizerState(&edgeRsDesc, &edgeRs)))
			return false;
		auto gsRsDesc = MakeRasterizerDesc(D3D11_CULL_NONE, true);
		gsRsDesc.DepthClipEnable = FALSE;
		gsRsDesc.DepthBias = -1;
		gsRsDesc.SlopeScaledDepthBias = -1.0f;
		gsRsDesc.DepthBiasClamp = -1.0f;
		if (FAILED(device->CreateRasterizerState(&gsRsDesc, &gsRs)))
			return false;
		auto gsDssDesc = MakeGroundShadowDepthStencilDesc();
		if (FAILED(device->CreateDepthStencilState(&gsDssDesc, &gsDss)))
			return false;
		auto defDssDesc  = MakeDefaultDepthStencilDesc();
		if (FAILED(device->CreateDepthStencilState(&defDssDesc, &defaultDss)))
			return false;
		return true;
	}

	bool Dx11Viewer::CreateDummyResources() {
		const auto d = MakeTexture2DDesc(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		if (FAILED(device->CreateTexture2D(&d, nullptr, &dummyTexture)))
			return false;
		if (FAILED(device->CreateShaderResourceView(dummyTexture.Get(), nullptr, &dummyTextureView)))
			return false;
		return true;
	}
}
