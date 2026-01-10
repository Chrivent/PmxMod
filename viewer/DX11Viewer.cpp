#include "DX11Viewer.h"

#include "../src/MMDReader.h"
#include "../src/MMDUtil.h"
#include "../src/MMDModel.h"

#include "../external/stb_image.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <ranges>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

bool DX11AppContext::Run(const SceneConfig& cfg) {
	MusicUtil music;
	music.Init(cfg.musicPath);
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
	D3D_FEATURE_LEVEL featureLevel;
	constexpr D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	UINT createFlags = 0;
	HRESULT hr = D3D11CreateDevice(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		createFlags,
		featureLevels, 1,
		D3D11_SDK_VERSION,
		&device, &featureLevel, &context
	);
	if (FAILED(hr))
		return false;
	Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
	hr = device.As(&dxgiDevice);
	if (FAILED(hr))
		return false;
	Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
	hr = dxgiDevice->GetAdapter(&adapter);
	if (FAILED(hr))
		return false;
	Microsoft::WRL::ComPtr<IDXGIFactory> factory;
	hr = adapter->GetParent(__uuidof(IDXGIFactory), &factory);
	if (FAILED(hr))
		return false;
	UINT msaaCount = 4;
	UINT quality = 0;
	hr = device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, msaaCount, &quality);
	if (FAILED(hr) || quality == 0) {
		msaaCount = 1;
		quality = 0;
	}
	UINT msaaQuality = quality > 0 ? quality - 1 : 0;
	if (!glfwInit())
		return false;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		return false;
	}
	const HWND hwnd = glfwGetWin32Window(window);
	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferCount = 2;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = msaaCount;
	sd.SampleDesc.Quality = msaaQuality;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	hr = factory->CreateSwapChain(device.Get(), &sd, &swapChain);
	if (FAILED(hr))
		return false;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
	auto CreateRenderTargets = [&](const int w, const int h) -> bool {
		rtv.Reset();
		depthTex.Reset();
		dsv.Reset();
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		HRESULT hr2 = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(backBuffer.GetAddressOf()));
		if (FAILED(hr2))
			return false;
		hr2 = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);
		if (FAILED(hr2))
			return false;
		D3D11_TEXTURE2D_DESC dsDesc{};
		dsDesc.Width = static_cast<UINT>(w);
		dsDesc.Height = static_cast<UINT>(h);
		dsDesc.MipLevels = 1;
		dsDesc.ArraySize = 1;
		dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsDesc.SampleDesc.Count = msaaCount;
		dsDesc.SampleDesc.Quality = msaaQuality;
		dsDesc.Usage = D3D11_USAGE_DEFAULT;
		dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		hr2 = device->CreateTexture2D(&dsDesc, nullptr, &depthTex);
		if (FAILED(hr2))
			return false;
		hr2 = device->CreateDepthStencilView(depthTex.Get(), nullptr, &dsv);
		if (FAILED(hr2))
			return false;
		return true;
	};
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	if (width <= 0 || height <= 0)
		return false;
	if (!CreateRenderTargets(width, height))
		return false;
	m_multiSampleCount = msaaCount;
	m_multiSampleQuality = msaaQuality;
	if (!Setup(device.Get()))
		return false;
	LoadCameraVmd(cfg);
	std::vector<std::unique_ptr<Model>> models;
	if (!LoadModels(cfg, models))
		return false;
	auto fpsTime  = std::chrono::steady_clock::now();
    auto saveTime = std::chrono::steady_clock::now();
    int fpsFrame  = 0;
    while (!glfwWindowShouldClose(window)) {
    	glfwPollEvents();
    	int newW = 0, newH = 0;
    	glfwGetFramebufferSize(window, &newW, &newH);
        if (newW <= 0 || newH <= 0)
        	continue;
        if (newW != width || newH != height) {
            width = newW; height = newH;
            rtv.Reset(); dsv.Reset(); depthTex.Reset();
			swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
            if (!CreateRenderTargets(width, height))
            	return false;
        }
    	StepTime(music, saveTime);
    	UpdateCamera(width, height);
        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);
        ID3D11RenderTargetView* rtvs[] = { rtv.Get() };
        context->OMSetRenderTargets(1, rtvs, dsv.Get());
	    constexpr float clearColor[] = { 0.839f, 0.902f, 0.961f, 1.0f };
        context->ClearRenderTargetView(rtv.Get(), clearColor);
        context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    	m_renderTargetView = rtv;
    	m_depthStencilView = dsv;
        for (auto& model : models) {
            model->UpdateAnimation(*this);
            model->Update();
            model->Draw(*this);
        	const auto& dx11Model = static_cast<DX11Model&>(*model);
        	Microsoft::WRL::ComPtr<ID3D11CommandList> cmd;
        	const HRESULT hrCL = dx11Model.m_context->FinishCommandList(FALSE, &cmd);
        	if (SUCCEEDED(hrCL) && cmd)
		        context->ExecuteCommandList(cmd.Get(), FALSE);
        }
        swapChain->Present(0, 0);
    	TickFps(fpsTime, fpsFrame);
    }
    models.clear();
	glfwDestroyWindow(window);
	glfwTerminate();
    return true;
}

std::unique_ptr<Model> DX11AppContext::CreateModel() const {
	return std::make_unique<DX11Model>();
}

bool DX11AppContext::Setup(const Microsoft::WRL::ComPtr<ID3D11Device>& device) {
	m_device = device;
	InitDirs("shader_DX11");
	if (!CreateShaders())
		return false;
	return true;
}

DX11Texture DX11AppContext::GetTexture(const std::filesystem::path& texturePath) {
	const auto it = m_textures.find(texturePath);
	if (it != m_textures.end())
		return it->second;
	int x = 0, y = 0, comp = 0;
	stbi_uc* image = LoadImageRGBA(texturePath, x, y, comp);
	if (!image)
		return {};
	D3D11_TEXTURE2D_DESC tex2dDesc = {};
	tex2dDesc.Width = x;
	tex2dDesc.Height = y;
	tex2dDesc.MipLevels = 1;
	tex2dDesc.ArraySize = 1;
	tex2dDesc.SampleDesc.Count = 1;
	tex2dDesc.SampleDesc.Quality = 0;
	tex2dDesc.Usage = D3D11_USAGE_DEFAULT;
	tex2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tex2dDesc.CPUAccessFlags = 0;
	tex2dDesc.MiscFlags = 0;
	const bool hasAlpha = comp == 4;
	tex2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = image;
	initData.SysMemPitch = 4 * x;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2d;
	HRESULT hr = m_device->CreateTexture2D(&tex2dDesc, &initData, &tex2d);
	stbi_image_free(image);
	if (FAILED(hr))
		return {};
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex2dRV;
	hr = m_device->CreateShaderResourceView(tex2d.Get(), nullptr, &tex2dRV);
	if (FAILED(hr))
		return {};
	DX11Texture tex;
	tex.m_texture = tex2d;
	tex.m_textureView = tex2dRV;
	tex.m_hasAlpha = hasAlpha;
	m_textures[texturePath] = tex;
	return m_textures[texturePath];
}

bool DX11AppContext::CreateShaders() {
	HRESULT hr;
	std::vector<uint8_t> mmdVSBytecode, mmdPSBytecode;
	std::vector<uint8_t> mmdEdgeVSBytecode, mmdEdgePSBytecode;
	std::vector<uint8_t> mmdGroundShadowVSBytecode, mmdGroundShadowPSBytecode;
	if (!ReadCsoBinary(m_shaderDir / "mmd_vs.cso", mmdVSBytecode))
		return false;
	if (!ReadCsoBinary(m_shaderDir / "mmd_ps.cso", mmdPSBytecode))
		return false;
	if (!ReadCsoBinary(m_shaderDir / "mmd_edge_vs.cso", mmdEdgeVSBytecode))
		return false;
	if (!ReadCsoBinary(m_shaderDir / "mmd_edge_ps.cso", mmdEdgePSBytecode))
		return false;
	if (!ReadCsoBinary(m_shaderDir / "mmd_gs_vs.cso", mmdGroundShadowVSBytecode))
		return false;
	if (!ReadCsoBinary(m_shaderDir / "mmd_gs_ps.cso", mmdGroundShadowPSBytecode))
		return false;

	// mmd shader_GLFW
	hr = m_device->CreateVertexShader(
		mmdVSBytecode.data(), mmdVSBytecode.size(),
		nullptr, &m_mmdVS);
	if (FAILED(hr))
		return false;
	hr = m_device->CreatePixelShader(
		mmdPSBytecode.data(), mmdPSBytecode.size(),
		nullptr, &m_mmdPS);
	if (FAILED(hr))
		return false;
	D3D11_INPUT_ELEMENT_DESC mmdInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = m_device->CreateInputLayout(
		mmdInputElementDesc, 3,
		mmdVSBytecode.data(), mmdVSBytecode.size(),
		&m_mmdInputLayout
	);
	if (FAILED(hr))
		return false;

	// Texture sampler
	D3D11_SAMPLER_DESC texSamplerDesc;
	texSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	texSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	texSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	texSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	texSamplerDesc.MinLOD = -FLT_MAX;
	texSamplerDesc.MaxLOD = -FLT_MAX;
	texSamplerDesc.MipLODBias = 0;
	texSamplerDesc.MaxAnisotropy = 0;
	hr = m_device->CreateSamplerState(&texSamplerDesc, &m_textureSampler);
	if (FAILED(hr))
		return false;

	// ToonTexture sampler
	D3D11_SAMPLER_DESC toonSamplerDesc;
	toonSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	toonSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	toonSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	toonSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	toonSamplerDesc.MinLOD = -FLT_MAX;
	toonSamplerDesc.MaxLOD = -FLT_MAX;
	toonSamplerDesc.MipLODBias = 0;
	toonSamplerDesc.MaxAnisotropy = 0;
	hr = m_device->CreateSamplerState(&toonSamplerDesc, &m_toonTextureSampler);
	if (FAILED(hr))
		return false;

	// SphereTexture sampler
	D3D11_SAMPLER_DESC spSamplerDesc;
	spSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	spSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	spSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	spSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	spSamplerDesc.MinLOD = -FLT_MAX;
	spSamplerDesc.MaxLOD = -FLT_MAX;
	spSamplerDesc.MipLODBias = 0;
	spSamplerDesc.MaxAnisotropy = 0;
	hr = m_device->CreateSamplerState(&spSamplerDesc, &m_sphereTextureSampler);
	if (FAILED(hr))
		return false;

	// Blend State
	D3D11_BLEND_DESC blendDesc1;
	blendDesc1.AlphaToCoverageEnable = false;
	blendDesc1.IndependentBlendEnable = false;
	blendDesc1.RenderTarget[0].BlendEnable = true;
	blendDesc1.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc1.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc1.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc1.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDesc1.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc1.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc1.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = m_device->CreateBlendState(&blendDesc1, &m_mmdBlendState);
	if (FAILED(hr))
		return false;

	// Rasterizer State (Front face)
	D3D11_RASTERIZER_DESC frontRsDesc;
	frontRsDesc.FillMode = D3D11_FILL_SOLID;
	frontRsDesc.CullMode = D3D11_CULL_BACK;
	frontRsDesc.FrontCounterClockwise = true;
	frontRsDesc.DepthBias = 0;
	frontRsDesc.SlopeScaledDepthBias = 0;
	frontRsDesc.DepthBiasClamp = 0;
	frontRsDesc.DepthClipEnable = false;
	frontRsDesc.ScissorEnable = false;
	frontRsDesc.MultisampleEnable = true;
	frontRsDesc.AntialiasedLineEnable = false;
	hr = m_device->CreateRasterizerState(&frontRsDesc, &m_mmdFrontFaceRS);
	if (FAILED(hr))
		return false;

	// Rasterizer State (Both faces)
	D3D11_RASTERIZER_DESC faceRsDesc;
	faceRsDesc.FillMode = D3D11_FILL_SOLID;
	faceRsDesc.CullMode = D3D11_CULL_NONE;
	faceRsDesc.FrontCounterClockwise = true;
	faceRsDesc.DepthBias = 0;
	faceRsDesc.SlopeScaledDepthBias = 0;
	faceRsDesc.DepthBiasClamp = 0;
	faceRsDesc.DepthClipEnable = false;
	faceRsDesc.ScissorEnable = false;
	faceRsDesc.MultisampleEnable = true;
	faceRsDesc.AntialiasedLineEnable = false;
	hr = m_device->CreateRasterizerState(&faceRsDesc, &m_mmdBothFaceRS);
	if (FAILED(hr))
		return false;

	// mmd edge shader_GLFW
	hr = m_device->CreateVertexShader(
		mmdEdgeVSBytecode.data(), mmdEdgeVSBytecode.size(),
		nullptr, &m_mmdEdgeVS);
	if (FAILED(hr))
		return false;

	hr = m_device->CreatePixelShader(
		mmdEdgePSBytecode.data(), mmdEdgePSBytecode.size(),
		nullptr, &m_mmdEdgePS);
	if (FAILED(hr))
		return false;

	D3D11_INPUT_ELEMENT_DESC mmdEdgeInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
			0, 0,
			D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,
			0, D3D11_APPEND_ALIGNED_ELEMENT,
			D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = m_device->CreateInputLayout(
		mmdEdgeInputElementDesc, 2,
		mmdEdgeVSBytecode.data(), mmdEdgeVSBytecode.size(),
		&m_mmdEdgeInputLayout
	);
	if (FAILED(hr))
		return false;

	// Blend State
	D3D11_BLEND_DESC blendDesc2;
	blendDesc2.AlphaToCoverageEnable = false;
	blendDesc2.IndependentBlendEnable = false;
	blendDesc2.RenderTarget[0].BlendEnable = true;
	blendDesc2.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc2.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc2.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc2.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDesc2.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc2.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc2.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = m_device->CreateBlendState(&blendDesc2, &m_mmdEdgeBlendState);
	if (FAILED(hr))
		return false;

	// Rasterizer State
	D3D11_RASTERIZER_DESC rsDesc1;
	rsDesc1.FillMode = D3D11_FILL_SOLID;
	rsDesc1.CullMode = D3D11_CULL_FRONT;
	rsDesc1.FrontCounterClockwise = true;
	rsDesc1.DepthBias = 0;
	rsDesc1.SlopeScaledDepthBias = 0;
	rsDesc1.DepthBiasClamp = 0;
	rsDesc1.DepthClipEnable = false;
	rsDesc1.ScissorEnable = false;
	rsDesc1.MultisampleEnable = true;
	rsDesc1.AntialiasedLineEnable = false;
	hr = m_device->CreateRasterizerState(&rsDesc1, &m_mmdEdgeRS);
	if (FAILED(hr))
		return false;

	// mmd ground shadow shader_GLFW
	hr = m_device->CreateVertexShader(
		mmdGroundShadowVSBytecode.data(), mmdGroundShadowVSBytecode.size(),
		nullptr, &m_mmdGroundShadowVS);
	if (FAILED(hr))
		return false;
	hr = m_device->CreatePixelShader(
		mmdGroundShadowPSBytecode.data(),mmdGroundShadowPSBytecode.size(),
		nullptr, &m_mmdGroundShadowPS);
	if (FAILED(hr))
		return false;

	D3D11_INPUT_ELEMENT_DESC mmdGroundShadowInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = m_device->CreateInputLayout(
		mmdGroundShadowInputElementDesc, 1,
		mmdGroundShadowVSBytecode.data(), mmdGroundShadowVSBytecode.size(),
		&m_mmdGroundShadowInputLayout
	);
	if (FAILED(hr))
		return false;

	// Blend State
	D3D11_BLEND_DESC blendDesc3;
	blendDesc3.AlphaToCoverageEnable = false;
	blendDesc3.IndependentBlendEnable = false;
	blendDesc3.RenderTarget[0].BlendEnable = true;
	blendDesc3.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc3.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc3.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc3.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDesc3.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc3.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc3.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = m_device->CreateBlendState(&blendDesc3, &m_mmdGroundShadowBlendState);
	if (FAILED(hr))
		return false;

	// Rasterizer State
	D3D11_RASTERIZER_DESC rsDesc2;
	rsDesc2.FillMode = D3D11_FILL_SOLID;
	rsDesc2.CullMode = D3D11_CULL_NONE;
	rsDesc2.FrontCounterClockwise = true;
	rsDesc2.DepthBias = -1;
	rsDesc2.SlopeScaledDepthBias = -1.0f;
	rsDesc2.DepthBiasClamp = -1.0f;
	rsDesc2.DepthClipEnable = false;
	rsDesc2.ScissorEnable = false;
	rsDesc2.MultisampleEnable = true;
	rsDesc2.AntialiasedLineEnable = false;
	hr = m_device->CreateRasterizerState(&rsDesc2, &m_mmdGroundShadowRS);
	if (FAILED(hr))
		return false;

	// Depth Stencil State
	D3D11_DEPTH_STENCIL_DESC stDesc;
	stDesc.DepthEnable = true;
	stDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	stDesc.DepthFunc = D3D11_COMPARISON_LESS;
	stDesc.StencilEnable = true;
	stDesc.StencilReadMask = 0x01;
	stDesc.StencilWriteMask = 0xFF;
	stDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
	stDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	stDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	stDesc.BackFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
	stDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	stDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	hr = m_device->CreateDepthStencilState(&stDesc, &m_mmdGroundShadowDSS);
	if (FAILED(hr))
		return false;

	// Default Depth Stencil State
	D3D11_DEPTH_STENCIL_DESC dstDesc;
	dstDesc.DepthEnable = true;
	dstDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dstDesc.DepthFunc = D3D11_COMPARISON_LESS;
	dstDesc.StencilEnable = false;
	dstDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	dstDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	dstDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	dstDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dstDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	dstDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dstDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	dstDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dstDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	dstDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	hr = m_device->CreateDepthStencilState(&dstDesc, &m_defaultDSS);
	if (FAILED(hr))
		return false;

	// Dummy texture
	D3D11_TEXTURE2D_DESC tex2dDesc = {};
	tex2dDesc.Width = 1;
	tex2dDesc.Height = 1;
	tex2dDesc.MipLevels = 1;
	tex2dDesc.ArraySize = 1;
	tex2dDesc.SampleDesc.Count = 1;
	tex2dDesc.SampleDesc.Quality = 0;
	tex2dDesc.Usage = D3D11_USAGE_DEFAULT;
	tex2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tex2dDesc.CPUAccessFlags = 0;
	tex2dDesc.MiscFlags = 0;
	tex2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	hr = m_device->CreateTexture2D(&tex2dDesc, nullptr, &m_dummyTexture);
	if (FAILED(hr))
		return false;

	hr = m_device->CreateShaderResourceView(m_dummyTexture.Get(), nullptr, &m_dummyTextureView);
	if (FAILED(hr))
		return false;

	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MinLOD = -FLT_MAX;
	samplerDesc.MaxLOD = -FLT_MAX;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MaxAnisotropy = 0;
	hr = m_device->CreateSamplerState(&samplerDesc, &m_dummySampler);
	if (FAILED(hr))
		return false;
	return true;
}

bool DX11AppContext::ReadCsoBinary(const std::filesystem::path& path, std::vector<uint8_t>& out) {
	std::ifstream f(path, std::ios::binary);
	if (!f)
		return false;
	f.seekg(0, std::ios::end);
	const auto size = static_cast<size_t>(f.tellg());
	f.seekg(0, std::ios::beg);
	out.resize(size);
	if (size > 0)
		f.read(reinterpret_cast<char*>(out.data()), static_cast<long long>(size));
	return true;
}

DX11Material::DX11Material(const MMDMaterial& mat)
	: m_mmdMat(mat) {
}

bool DX11Model::Setup(AppContext& appContext) {
	auto& dx11AppContext = static_cast<DX11AppContext&>(appContext);
	HRESULT hr;
	hr = dx11AppContext.m_device->CreateDeferredContext(0, &m_context);
	if (FAILED(hr))
		return false;

	// Setup vertex buffer
	D3D11_BUFFER_DESC vBufDesc = {};
	vBufDesc.Usage = D3D11_USAGE_DYNAMIC;
	vBufDesc.ByteWidth = static_cast<UINT>(sizeof(DX11Vertex) * m_mmdModel->m_positions.size());
	vBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = dx11AppContext.m_device->CreateBuffer(&vBufDesc, nullptr, &m_vertexBuffer);
	if (FAILED(hr))
		return false;

	// Setup index buffer;
	D3D11_BUFFER_DESC iBufDesc = {};
	iBufDesc.Usage = D3D11_USAGE_IMMUTABLE;
	iBufDesc.ByteWidth = static_cast<UINT>(m_mmdModel->m_indexElementSize * m_mmdModel->m_indexCount);
	iBufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	iBufDesc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = &m_mmdModel->m_indices[0];
	hr = dx11AppContext.m_device->CreateBuffer(&iBufDesc, &initData, &m_indexBuffer);
	if (FAILED(hr))
		return false;
	if (1 == m_mmdModel->m_indexElementSize)
		m_indexBufferFormat = DXGI_FORMAT_R8_UINT;
	else if (2 == m_mmdModel->m_indexElementSize)
		m_indexBufferFormat = DXGI_FORMAT_R16_UINT;
	else if (4 == m_mmdModel->m_indexElementSize)
		m_indexBufferFormat = DXGI_FORMAT_R32_UINT;
	else
		return false;

	// Setup mmd vertex shader constant buffer (VSData)
	D3D11_BUFFER_DESC vsBufDesc = {};
	vsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	vsBufDesc.ByteWidth = sizeof(DX11VertexShader);
	vsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	vsBufDesc.CPUAccessFlags = 0;
	hr = dx11AppContext.m_device->CreateBuffer(&vsBufDesc, nullptr, &m_mmdVSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd pixel shader constant buffer (PSData)
	D3D11_BUFFER_DESC psBufDesc = {};
	psBufDesc.Usage = D3D11_USAGE_DEFAULT;
	psBufDesc.ByteWidth = sizeof(DX11PixelShader);
	psBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	psBufDesc.CPUAccessFlags = 0;
	hr = dx11AppContext.m_device->CreateBuffer(&psBufDesc, nullptr, &m_mmdPSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd edge vertex shader constant buffer (VSData)
	D3D11_BUFFER_DESC evsBufDesc1 = {};
	evsBufDesc1.Usage = D3D11_USAGE_DEFAULT;
	evsBufDesc1.ByteWidth = sizeof(DX11EdgeVertexShader);
	evsBufDesc1.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	evsBufDesc1.CPUAccessFlags = 0;
	hr = dx11AppContext.m_device->CreateBuffer(&evsBufDesc1, nullptr, &m_mmdEdgeVSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd edge vertex shader constant buffer (VSEdgeData)
	D3D11_BUFFER_DESC evsBufDesc2 = {};
	evsBufDesc2.Usage = D3D11_USAGE_DEFAULT;
	evsBufDesc2.ByteWidth = sizeof(DX11EdgeSizeVertexShader);
	evsBufDesc2.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	evsBufDesc2.CPUAccessFlags = 0;
	hr = dx11AppContext.m_device->CreateBuffer(&evsBufDesc2, nullptr, &m_mmdEdgeSizeVSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd edge pixel shader constant buffer (PSData)
	D3D11_BUFFER_DESC epsBufDesc = {};
	epsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	epsBufDesc.ByteWidth = sizeof(DX11EdgePixelShader);
	epsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	epsBufDesc.CPUAccessFlags = 0;
	hr = dx11AppContext.m_device->CreateBuffer(&epsBufDesc, nullptr, &m_mmdEdgePSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd ground shadow vertex shader constant buffer (VSData)
	D3D11_BUFFER_DESC gvsBufDesc = {};
	gvsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	gvsBufDesc.ByteWidth = sizeof(DX11GroundShadowVertexShader);
	gvsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	gvsBufDesc.CPUAccessFlags = 0;
	hr = dx11AppContext.m_device->CreateBuffer(&gvsBufDesc, nullptr, &m_mmdGroundShadowVSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd ground shadow pixel shader constant buffer (PSData)
	D3D11_BUFFER_DESC gpsBufDesc = {};
	gpsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	gpsBufDesc.ByteWidth = sizeof(DX11GroundShadowPixelShader);
	gpsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	gpsBufDesc.CPUAccessFlags = 0;
	hr = dx11AppContext.m_device->CreateBuffer(&gpsBufDesc, nullptr, &m_mmdGroundShadowPSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup materials
	for (const auto& mmdMat : m_mmdModel->m_materials) {
		DX11Material mat(mmdMat);
		if (!mmdMat.m_texture.empty())
			mat.m_texture = dx11AppContext.GetTexture(mmdMat.m_texture);
		if (!mmdMat.m_spTexture.empty())
			mat.m_spTexture = dx11AppContext.GetTexture(mmdMat.m_spTexture);
		if (!mmdMat.m_toonTexture.empty())
			mat.m_toonTexture = dx11AppContext.GetTexture(mmdMat.m_toonTexture);
		m_materials.emplace_back(std::move(mat));
	}
	return true;
}

void DX11Model::Update() const {
	m_mmdModel->Update();
	const size_t vtxCount = m_mmdModel->m_positions.size();
	D3D11_MAPPED_SUBRESOURCE mapRes;
	const HRESULT hr = m_context->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapRes);
	if (FAILED(hr))
		return;
	const auto vertices = static_cast<DX11Vertex*>(mapRes.pData);
	const glm::vec3* positions = m_mmdModel->m_updatePositions.data();
	const glm::vec3* normals = m_mmdModel->m_updateNormals.data();
	const glm::vec2* uvs = m_mmdModel->m_updateUVs.data();
	for (size_t i = 0; i < vtxCount; i++) {
		vertices[i].m_position = positions[i];
		vertices[i].m_normal = normals[i];
		vertices[i].m_uv = uvs[i];
	}
	m_context->Unmap(m_vertexBuffer.Get(), 0);
}

void DX11Model::Draw(AppContext& appContext) const {
	auto& dx11AppContext = static_cast<DX11AppContext&>(appContext);
	const auto& view = appContext.m_viewMat;
	const auto& proj = appContext.m_projMat;
	const auto& dxMat = glm::mat4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.5f, 1.0f
	);
	auto world = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));
	auto wv = view * world;
	auto wvp = dxMat * proj * view * world;

	// Set viewport
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<float>(appContext.m_screenWidth);
	vp.Height = static_cast<float>(appContext.m_screenHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_context->RSSetViewports(1, &vp);
	ID3D11RenderTargetView* rtvs[] = { dx11AppContext.m_renderTargetView.Get() };
	m_context->OMSetRenderTargets(1, rtvs, dx11AppContext.m_depthStencilView.Get());
	m_context->OMSetDepthStencilState(dx11AppContext.m_defaultDSS.Get(), 0x00);

	// Setup input assembler
	UINT strides[] = { sizeof(DX11Vertex) };
	UINT offsets[] = { 0 };
	m_context->IASetInputLayout(dx11AppContext.m_mmdInputLayout.Get());
	ID3D11Buffer* vbs[] = { m_vertexBuffer.Get() };
	m_context->IASetVertexBuffers(0, 1, vbs, strides, offsets);
	m_context->IASetIndexBuffer(m_indexBuffer.Get(), m_indexBufferFormat, 0);
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Draw model
	DX11VertexShader vsCB1{};
	vsCB1.m_wv = wv;
	vsCB1.m_wvp = wvp;
	m_context->UpdateSubresource(m_mmdVSConstantBuffer.Get(),
								 0, nullptr, &vsCB1, 0, 0);
	m_context->VSSetShader(dx11AppContext.m_mmdVS.Get(), nullptr, 0);
	ID3D11Buffer* cbs1[] = { m_mmdVSConstantBuffer.Get() };
	m_context->VSSetConstantBuffers(0, 1, cbs1);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		if (mat.m_mmdMat.m_diffuse.a == 0)
			continue;
		m_context->PSSetShader(dx11AppContext.m_mmdPS.Get(), nullptr, 0);
		DX11PixelShader psCB{};
		psCB.m_alpha = mmdMat.m_diffuse.a;
		psCB.m_diffuse = mmdMat.m_diffuse;
		psCB.m_ambient = mmdMat.m_ambient;
		psCB.m_specular = mmdMat.m_specular;
		psCB.m_specularPower = mmdMat.m_specularPower;
		if (mat.m_texture.m_texture) {
			if (!mat.m_texture.m_hasAlpha)
				psCB.m_textureModes.x = 1;
			else
				psCB.m_textureModes.x = 2;
			psCB.m_texMulFactor = mmdMat.m_textureMulFactor;
			psCB.m_texAddFactor = mmdMat.m_textureAddFactor;
			ID3D11ShaderResourceView* views[] = { mat.m_texture.m_textureView.Get() };
			ID3D11SamplerState* samplers[] = { dx11AppContext.m_textureSampler.Get() };
			m_context->PSSetShaderResources(0, 1, views);
			m_context->PSSetSamplers(0, 1, samplers);
		} else {
			psCB.m_textureModes.x = 0;
			ID3D11ShaderResourceView* views[] = { dx11AppContext.m_dummyTextureView.Get() };
			ID3D11SamplerState* samplers[] = { dx11AppContext.m_dummySampler.Get() };
			m_context->PSSetShaderResources(0, 1, views);
			m_context->PSSetSamplers(0, 1, samplers);
		}
		if (mat.m_toonTexture.m_texture) {
			psCB.m_textureModes.y = 1;
			psCB.m_toonTexMulFactor = mmdMat.m_toonTextureMulFactor;
			psCB.m_toonTexAddFactor = mmdMat.m_toonTextureAddFactor;
			ID3D11ShaderResourceView* views[] = { mat.m_toonTexture.m_textureView.Get() };
			ID3D11SamplerState* samplers[] = { dx11AppContext.m_toonTextureSampler.Get() };
			m_context->PSSetShaderResources(1, 1, views);
			m_context->PSSetSamplers(1, 1, samplers);
		} else {
			psCB.m_textureModes.y = 0;
			ID3D11ShaderResourceView* views[] = { dx11AppContext.m_dummyTextureView.Get() };
			ID3D11SamplerState* samplers[] = { dx11AppContext.m_dummySampler.Get() };
			m_context->PSSetShaderResources(1, 1, views);
			m_context->PSSetSamplers(1, 1, samplers);
		}
		if (mat.m_spTexture.m_texture) {
			if (mmdMat.m_spTextureMode == SphereMode::Mul)
				psCB.m_textureModes.z = 1;
			else if (mmdMat.m_spTextureMode == SphereMode::Add)
				psCB.m_textureModes.z = 2;
			psCB.m_sphereTexMulFactor = mmdMat.m_spTextureMulFactor;
			psCB.m_sphereTexAddFactor = mmdMat.m_spTextureAddFactor;
			ID3D11ShaderResourceView* views[] = { mat.m_spTexture.m_textureView.Get() };
			ID3D11SamplerState* samplers[] = { dx11AppContext.m_sphereTextureSampler.Get() };
			m_context->PSSetShaderResources(2, 1, views);
			m_context->PSSetSamplers(2, 1, samplers);
		} else {
			psCB.m_textureModes.z = 0;
			ID3D11ShaderResourceView* views[] = { dx11AppContext.m_dummyTextureView.Get() };
			ID3D11SamplerState* samplers[] = { dx11AppContext.m_dummySampler.Get() };
			m_context->PSSetShaderResources(2, 1, views);
			m_context->PSSetSamplers(2, 1, samplers);
		}
		psCB.m_lightColor = appContext.m_lightColor;
		glm::vec3 lightDir = appContext.m_lightDir;
		auto viewMat = glm::mat3(appContext.m_viewMat);
		lightDir = viewMat * lightDir;
		psCB.m_lightDir = lightDir;
		m_context->UpdateSubresource(m_mmdPSConstantBuffer.Get(),
									 0, nullptr, &psCB, 0, 0);
		ID3D11Buffer* pscbs[] = { m_mmdPSConstantBuffer.Get() };
		m_context->PSSetConstantBuffers(1, 1, pscbs);
		if (mmdMat.m_bothFace)
			m_context->RSSetState(dx11AppContext.m_mmdBothFaceRS.Get());
		else
			m_context->RSSetState(dx11AppContext.m_mmdFrontFaceRS.Get());
		m_context->OMSetBlendState(dx11AppContext.m_mmdBlendState.Get(), nullptr, 0xffffffff);
		m_context->DrawIndexed(m_vertexCount, m_beginIndex, 0);
	}
	ID3D11ShaderResourceView* views[] = { nullptr, nullptr, nullptr };
	ID3D11SamplerState* samplers[] = { nullptr, nullptr, nullptr };
	m_context->PSSetShaderResources(0, 3, views);
	m_context->PSSetSamplers(0, 3, samplers);

	// Draw edge
	m_context->IASetInputLayout(dx11AppContext.m_mmdEdgeInputLayout.Get());
	DX11EdgeVertexShader vsCB2{};
	vsCB2.m_wv = wv;
	vsCB2.m_wvp = wvp;
	vsCB2.m_screenSize = glm::vec2(static_cast<float>(appContext.m_screenWidth),
								   static_cast<float>(appContext.m_screenHeight));
	m_context->UpdateSubresource(m_mmdEdgeVSConstantBuffer.Get(),
								 0, nullptr, &vsCB2, 0, 0);
	m_context->VSSetShader(dx11AppContext.m_mmdEdgeVS.Get(), nullptr, 0);
	ID3D11Buffer* cbs2[] = { m_mmdEdgeVSConstantBuffer.Get() };
	m_context->VSSetConstantBuffers(0, 1, cbs2);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		if (!mmdMat.m_edgeFlag)
			continue;
		if (mat.m_mmdMat.m_diffuse.a == 0)
			continue;
		DX11EdgeSizeVertexShader vsCB{};
		vsCB.m_edgeSize = mmdMat.m_edgeSize;
		m_context->UpdateSubresource(m_mmdEdgeSizeVSConstantBuffer.Get(),
									 0, nullptr, &vsCB, 0, 0);
		ID3D11Buffer* cbs[] = { m_mmdEdgeSizeVSConstantBuffer.Get() };
		m_context->VSSetConstantBuffers(1, 1, cbs);
		m_context->PSSetShader(dx11AppContext.m_mmdEdgePS.Get(), nullptr, 0);
		DX11EdgePixelShader psCB{};
		psCB.m_edgeColor = mmdMat.m_edgeColor;
		m_context->UpdateSubresource(m_mmdEdgePSConstantBuffer.Get(),
									 0, nullptr, &psCB, 0, 0);
		ID3D11Buffer* pscbs[] = { m_mmdEdgePSConstantBuffer.Get() };
		m_context->PSSetConstantBuffers(2, 1, pscbs);
		m_context->RSSetState(dx11AppContext.m_mmdEdgeRS.Get());
		m_context->OMSetBlendState(dx11AppContext.m_mmdEdgeBlendState.Get(), nullptr, 0xffffffff);
		m_context->DrawIndexed(m_vertexCount, m_beginIndex, 0);
	}

	// Draw ground shadow
	m_context->IASetInputLayout(dx11AppContext.m_mmdGroundShadowInputLayout.Get());
	auto plane = glm::vec4(0, 1, 0, 0);
	auto light = -appContext.m_lightDir;
	auto shadow = glm::mat4(1);
	shadow[0][0] = plane.y * light.y + plane.z * light.z;
	shadow[0][1] = -plane.x * light.y;
	shadow[0][2] = -plane.x * light.z;
	shadow[0][3] = 0;
	shadow[1][0] = -plane.y * light.x;
	shadow[1][1] = plane.x * light.x + plane.z * light.z;
	shadow[1][2] = -plane.y * light.z;
	shadow[1][3] = 0;
	shadow[2][0] = -plane.z * light.x;
	shadow[2][1] = -plane.z * light.y;
	shadow[2][2] = plane.x * light.x + plane.y * light.y;
	shadow[2][3] = 0;
	shadow[3][0] = -plane.w * light.x;
	shadow[3][1] = -plane.w * light.y;
	shadow[3][2] = -plane.w * light.z;
	shadow[3][3] = plane.x * light.x + plane.y * light.y + plane.z * light.z;
	auto wsvp = dxMat * proj * view * shadow * world;
	DX11GroundShadowVertexShader vsCB{};
	vsCB.m_wvp = wsvp;
	m_context->UpdateSubresource(m_mmdGroundShadowVSConstantBuffer.Get(),
								 0, nullptr, &vsCB, 0, 0);
	m_context->VSSetShader(dx11AppContext.m_mmdGroundShadowVS.Get(), nullptr, 0);
	ID3D11Buffer* cbs[] = { m_mmdGroundShadowVSConstantBuffer.Get() };
	m_context->VSSetConstantBuffers(0, 1, cbs);
	m_context->RSSetState(dx11AppContext.m_mmdGroundShadowRS.Get());
	m_context->OMSetBlendState(dx11AppContext.m_mmdGroundShadowBlendState.Get(), nullptr, 0xffffffff);
	m_context->OMSetDepthStencilState(dx11AppContext.m_mmdGroundShadowDSS.Get(), 0x01);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		if (!mmdMat.m_groundShadow)
			continue;
		if (mat.m_mmdMat.m_diffuse.a == 0)
			continue;
		m_context->PSSetShader(dx11AppContext.m_mmdGroundShadowPS.Get(), nullptr, 0);
		DX11GroundShadowPixelShader psCB{};
		psCB.m_shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
		m_context->UpdateSubresource(m_mmdGroundShadowPSConstantBuffer.Get(), 0, nullptr, &psCB, 0, 0);
		ID3D11Buffer* pscbs[] = { m_mmdGroundShadowPSConstantBuffer.Get() };
		m_context->PSSetConstantBuffers(1, 1, pscbs);
		m_context->OMSetBlendState(dx11AppContext.m_mmdEdgeBlendState.Get(), nullptr, 0xffffffff);
		m_context->DrawIndexed(m_vertexCount, m_beginIndex, 0);
	}
}
