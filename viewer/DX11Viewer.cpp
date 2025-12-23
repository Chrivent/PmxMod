#include "DX11Viewer.h"

#include "../src/MMDUtil.h"
#include "../src/MMDModel.h"

#include "../external/stb_image.h"

#include <d3dcompiler.h>
#include <fstream>

bool AppContext::Setup(const Microsoft::WRL::ComPtr<ID3D11Device>& device) {
    m_device = device;
    m_resourceDir = PathUtil::GetExecutablePath();
    m_resourceDir = m_resourceDir.parent_path();
    m_resourceDir /= "resource";
    m_shaderDir = m_resourceDir / "shader_DX11";
    m_mmdDir = m_resourceDir / "mmd";
    if (!CreateShaders())
        return false;
    return true;
}

bool AppContext::CreateShaders() {
	auto ReadFileBinary = [](const std::filesystem::path& path, std::vector<uint8_t>& out) {
		std::ifstream f(path, std::ios::binary);
		if (!f)
			return false;
		f.seekg(0, std::ios::end);
		const auto size = static_cast<size_t>(f.tellg());
		f.seekg(0, std::ios::beg);
		out.resize(size);
		if (size > 0)
			f.read(reinterpret_cast<char*>(out.data()), size);
		return true;
	};
	HRESULT hr;
	std::vector<uint8_t> mmdVSBytecode, mmdPSBytecode;
	std::vector<uint8_t> mmdEdgeVSBytecode, mmdEdgePSBytecode;
	std::vector<uint8_t> mmdGroundShadowVSBytecode, mmdGroundShadowPSBytecode;
	if (!ReadFileBinary(m_shaderDir / "mmd.vs.cso", mmdVSBytecode))
		return false;
	if (!ReadFileBinary(m_shaderDir / "mmd.ps.cso", mmdPSBytecode))
		return false;
	if (!ReadFileBinary(m_shaderDir / "mmd_edge.vs.cso", mmdEdgeVSBytecode))
		return false;
	if (!ReadFileBinary(m_shaderDir / "mmd_edge.ps.cso", mmdEdgePSBytecode))
		return false;
	if (!ReadFileBinary(m_shaderDir / "mmd_ground_shadow.vs.cso", mmdGroundShadowVSBytecode))
		return false;
	if (!ReadFileBinary(m_shaderDir / "mmd_ground_shadow.ps.cso", mmdGroundShadowPSBytecode))
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
	D3D11_SAMPLER_DESC texSamplerDesc = {};
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
	D3D11_SAMPLER_DESC toonSamplerDesc = {};
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
	D3D11_SAMPLER_DESC spSamplerDesc = {};
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
	D3D11_BLEND_DESC blendDesc1 = {};
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
	D3D11_RASTERIZER_DESC frontRsDesc = {};
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

	// Rasterizer State (Both face)
	D3D11_RASTERIZER_DESC faceRsDesc = {};
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
	D3D11_BLEND_DESC blendDesc2 = {};
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
	D3D11_RASTERIZER_DESC rsDesc1 = {};
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
	D3D11_BLEND_DESC blendDesc3 = {};
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
	D3D11_RASTERIZER_DESC rsDesc2 = {};
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
	D3D11_DEPTH_STENCIL_DESC stDesc = {};
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
	D3D11_DEPTH_STENCIL_DESC dstDesc = {};
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

	D3D11_SAMPLER_DESC samplerDesc = {};
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

Texture AppContext::GetTexture(const std::filesystem::path& texturePath) {
	const auto it = m_textures.find(texturePath);
	if (it != m_textures.end())
		return it->second;
	FILE* fp = nullptr;
	if (_wfopen_s(&fp, texturePath.c_str(), L"rb") != 0 || !fp)
		return Texture();
	int x = 0, y = 0, comp = 0;
	stbi_uc* image = stbi_load_from_file(fp, &x, &y, &comp, STBI_rgb_alpha);
	std::fclose(fp);
	if (!image)
		return Texture();
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
		return Texture();
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex2dRV;
	hr = m_device->CreateShaderResourceView(tex2d.Get(), nullptr, &tex2dRV);
	if (FAILED(hr))
		return Texture();
	Texture tex;
	tex.m_texture = tex2d;
	tex.m_textureView = tex2dRV;
	tex.m_hasAlpha = hasAlpha;
	m_textures[texturePath] = tex;
	return m_textures[texturePath];
}

Material::Material(const MMDMaterial& mat)
	: m_mmdMat(mat) {
}

bool Model::Setup(AppContext& appContext) {
	HRESULT hr;
	hr = appContext.m_device->CreateDeferredContext(0, &m_context);
	if (FAILED(hr))
		return false;

	// Setup vertex buffer
	D3D11_BUFFER_DESC vBufDesc = {};
	vBufDesc.Usage = D3D11_USAGE_DYNAMIC;
	vBufDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * m_mmdModel->m_positions.size());
	vBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = appContext.m_device->CreateBuffer(&vBufDesc, nullptr, &m_vertexBuffer);
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
	hr = appContext.m_device->CreateBuffer(&iBufDesc, &initData, &m_indexBuffer);
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
	vsBufDesc.ByteWidth = sizeof(MMDVertexShaderCB);
	vsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	vsBufDesc.CPUAccessFlags = 0;
	hr = appContext.m_device->CreateBuffer(&vsBufDesc, nullptr, &m_mmdVSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd pixel shader constant buffer (PSData)
	D3D11_BUFFER_DESC psBufDesc = {};
	psBufDesc.Usage = D3D11_USAGE_DEFAULT;
	psBufDesc.ByteWidth = sizeof(MMDPixelShaderCB);
	psBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	psBufDesc.CPUAccessFlags = 0;
	hr = appContext.m_device->CreateBuffer(&psBufDesc, nullptr, &m_mmdPSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd edge vertex shader constant buffer (VSData)
	D3D11_BUFFER_DESC evsBufDesc1 = {};
	evsBufDesc1.Usage = D3D11_USAGE_DEFAULT;
	evsBufDesc1.ByteWidth = sizeof(MMDEdgeVertexShaderCB);
	evsBufDesc1.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	evsBufDesc1.CPUAccessFlags = 0;
	hr = appContext.m_device->CreateBuffer(&evsBufDesc1, nullptr, &m_mmdEdgeVSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd edge vertex shader constant buffer (VSEdgeData)
	D3D11_BUFFER_DESC evsBufDesc2 = {};
	evsBufDesc2.Usage = D3D11_USAGE_DEFAULT;
	evsBufDesc2.ByteWidth = sizeof(MMDEdgeSizeVertexShaderCB);
	evsBufDesc2.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	evsBufDesc2.CPUAccessFlags = 0;
	hr = appContext.m_device->CreateBuffer(&evsBufDesc2, nullptr, &m_mmdEdgeSizeVSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd edge pixel shader constant buffer (PSData)
	D3D11_BUFFER_DESC epsBufDesc = {};
	epsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	epsBufDesc.ByteWidth = sizeof(MMDEdgePixelShaderCB);
	epsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	epsBufDesc.CPUAccessFlags = 0;
	hr = appContext.m_device->CreateBuffer(&epsBufDesc, nullptr, &m_mmdEdgePSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd ground shadow vertex shader constant buffer (VSData)
	D3D11_BUFFER_DESC gvsBufDesc = {};
	gvsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	gvsBufDesc.ByteWidth = sizeof(MMDGroundShadowVertexShaderCB);
	gvsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	gvsBufDesc.CPUAccessFlags = 0;
	hr = appContext.m_device->CreateBuffer(&gvsBufDesc, nullptr, &m_mmdGroundShadowVSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup mmd ground shadow pixel shader constant buffer (PSData)
	D3D11_BUFFER_DESC gpsBufDesc = {};
	gpsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	gpsBufDesc.ByteWidth = sizeof(MMDGroundShadowPixelShaderCB);
	gpsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	gpsBufDesc.CPUAccessFlags = 0;
	hr = appContext.m_device->CreateBuffer(&gpsBufDesc, nullptr, &m_mmdGroundShadowPSConstantBuffer);
	if (FAILED(hr))
		return false;

	// Setup materials
	for (const auto& mmdMat : m_mmdModel->m_materials) {
		Material mat(mmdMat);
		if (!mmdMat.m_texture.empty())
			mat.m_texture = appContext.GetTexture(mmdMat.m_texture);
		if (!mmdMat.m_spTexture.empty())
			mat.m_spTexture = appContext.GetTexture(mmdMat.m_spTexture);
		if (!mmdMat.m_toonTexture.empty())
			mat.m_toonTexture = appContext.GetTexture(mmdMat.m_toonTexture);
		m_materials.emplace_back(std::move(mat));
	}

	return true;
}
