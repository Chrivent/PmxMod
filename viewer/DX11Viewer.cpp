#include "DX11Viewer.h"

#include "../src/MMDReader.h"
#include "../src/MMDUtil.h"
#include "../src/MMDModel.h"

#include "../external/stb_image.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <d3dcompiler.h>
#include <iostream>
#include <ranges>

bool CompileShader(const std::filesystem::path& p, const char* entry, const char* target, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob) {
	Microsoft::WRL::ComPtr<ID3DBlob> err;
	if (FAILED(D3DCompileFromFile(p.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
		&outBlob, &err))) {
		if (err)
			std::cout << "Compile Error: " << static_cast<char*>(err->GetBufferPointer()) << "\n";
		return false;
	}
	return true;
}

D3D11_SAMPLER_DESC MakeSamplerDesc(
    const D3D11_FILTER filter,
    const D3D11_TEXTURE_ADDRESS_MODE addrU,
    const D3D11_TEXTURE_ADDRESS_MODE addrV,
    const D3D11_TEXTURE_ADDRESS_MODE addrW) {
    D3D11_SAMPLER_DESC d;
    d.Filter = filter;
    d.AddressU = addrU;
    d.AddressV = addrV;
    d.AddressW = addrW;
    d.MipLODBias = 0.0f;
    d.MaxAnisotropy = 0;
    d.ComparisonFunc = D3D11_COMPARISON_NEVER;
    d.BorderColor[0] = d.BorderColor[1] = d.BorderColor[2] = d.BorderColor[3] = 0.0f;
    d.MinLOD = 0.0f;
    d.MaxLOD = D3D11_FLOAT32_MAX;
    return d;
}

D3D11_BLEND_DESC MakeAlphaBlendDesc() {
    D3D11_BLEND_DESC d;
    d.AlphaToCoverageEnable = FALSE;
    d.IndependentBlendEnable = FALSE;
    auto& [BlendEnable, SrcBlend, DestBlend, BlendOp,
    	SrcBlendAlpha, DestBlendAlpha, BlendOpAlpha, RenderTargetWriteMask] = d.RenderTarget[0];
    BlendEnable = TRUE;
    SrcBlend = D3D11_BLEND_SRC_ALPHA;
    DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendOp = D3D11_BLEND_OP_ADD;
    SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    BlendOpAlpha = D3D11_BLEND_OP_ADD;
    RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    return d;
}

D3D11_RASTERIZER_DESC MakeRasterDesc(
    const D3D11_CULL_MODE cull,
    const bool frontCCW,
    const int depthBias = 0,
    const float slopeScaledDepthBias = 0.0f,
    const float depthBiasClamp = 0.0f) {
    D3D11_RASTERIZER_DESC d;
    d.FillMode = D3D11_FILL_SOLID;
    d.CullMode = cull;
    d.FrontCounterClockwise = frontCCW ? TRUE : FALSE;
    d.DepthBias = depthBias;
    d.SlopeScaledDepthBias = slopeScaledDepthBias;
    d.DepthBiasClamp = depthBiasClamp;
    d.DepthClipEnable = TRUE;
    d.ScissorEnable = FALSE;
    d.MultisampleEnable = TRUE;
    d.AntialiasedLineEnable = FALSE;
    return d;
}

D3D11_DEPTH_STENCIL_DESC MakeDefaultDepthStencilDesc() {
    D3D11_DEPTH_STENCIL_DESC d;
    d.DepthEnable = TRUE;
    d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    d.DepthFunc = D3D11_COMPARISON_LESS;
    d.StencilEnable = FALSE;
    d.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    d.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    d.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    d.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    d.BackFace = d.FrontFace;
    return d;
}

D3D11_DEPTH_STENCIL_DESC MakeGroundShadowDepthStencilDesc() {
    D3D11_DEPTH_STENCIL_DESC d;
    d.DepthEnable = TRUE;
    d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    d.DepthFunc = D3D11_COMPARISON_LESS;
    d.StencilEnable = TRUE;
    d.StencilReadMask = 0x01;
    d.StencilWriteMask = 0xFF;
    d.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
    d.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
    d.BackFace = d.FrontFace;
    return d;
}

template<class T>
HRESULT CreateConstBuffer(ID3D11Device* dev, Microsoft::WRL::ComPtr<ID3D11Buffer>& out) {
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = static_cast<UINT>(sizeof(T) + 15 & ~15);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	return dev->CreateBuffer(&bd, nullptr, &out);
}

void BindOrDummyPS(
	ID3D11DeviceContext* ctx,
	const UINT slot,
	const DX11Texture& tex,
	ID3D11SamplerState* sampler,
	ID3D11ShaderResourceView* dummySRV,
	ID3D11SamplerState* dummySampler) {
	ID3D11ShaderResourceView* views[] = { tex.m_texture ? tex.m_textureView.Get() : dummySRV };
	ID3D11SamplerState* samplers[] = { tex.m_texture ? sampler : dummySampler };
	ctx->PSSetShaderResources(slot, 1, views);
	ctx->PSSetSamplers(slot, 1, samplers);
}

DX11Material::DX11Material(const MMDMaterial& mat)
	: m_mmdMat(mat) {
}

bool DX11Model::Setup(Viewer& viewer) {
	auto& dx11Viewer = dynamic_cast<DX11Viewer&>(viewer);
	if (FAILED(dx11Viewer.m_device->CreateDeferredContext(0, &m_context)))
		return false;
	D3D11_BUFFER_DESC vBufDesc = {};
	vBufDesc.Usage = D3D11_USAGE_DYNAMIC;
	vBufDesc.ByteWidth = static_cast<UINT>(sizeof(DX11Vertex) * m_mmdModel->m_positions.size());
	vBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(dx11Viewer.m_device->CreateBuffer(&vBufDesc, nullptr, &m_vertexBuffer)))
		return false;
	D3D11_BUFFER_DESC iBufDesc = {};
	iBufDesc.Usage = D3D11_USAGE_IMMUTABLE;
	iBufDesc.ByteWidth = static_cast<UINT>(m_mmdModel->m_indexElementSize * m_mmdModel->m_indexCount);
	iBufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	iBufDesc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = &m_mmdModel->m_indices[0];
	if (FAILED(dx11Viewer.m_device->CreateBuffer(&iBufDesc, &initData, &m_indexBuffer)))
		return false;
	if (1 == m_mmdModel->m_indexElementSize)
		m_indexBufferFormat = DXGI_FORMAT_R8_UINT;
	else if (2 == m_mmdModel->m_indexElementSize)
		m_indexBufferFormat = DXGI_FORMAT_R16_UINT;
	else if (4 == m_mmdModel->m_indexElementSize)
		m_indexBufferFormat = DXGI_FORMAT_R32_UINT;
	else
		return false;
	if (FAILED(CreateConstBuffer<DX11VertexShader>(dx11Viewer.m_device.Get(), m_mmdVSConstantBuffer)))
		return false;
	if (FAILED(CreateConstBuffer<DX11PixelShader>(dx11Viewer.m_device.Get(), m_mmdPSConstantBuffer)))
		return false;
	if (FAILED(CreateConstBuffer<DX11EdgeVertexShader>(dx11Viewer.m_device.Get(), m_mmdEdgeVSConstantBuffer)))
		return false;
	if (FAILED(CreateConstBuffer<DX11EdgeSizeVertexShader>(dx11Viewer.m_device.Get(), m_mmdEdgeSizeVSConstantBuffer)))
		return false;
	if (FAILED(CreateConstBuffer<DX11EdgePixelShader>(dx11Viewer.m_device.Get(), m_mmdEdgePSConstantBuffer)))
		return false;
	if (FAILED(CreateConstBuffer<DX11GroundShadowVertexShader>(dx11Viewer.m_device.Get(), m_mmdGroundShadowVSConstantBuffer)))
		return false;
	if (FAILED(CreateConstBuffer<DX11GroundShadowPixelShader>(dx11Viewer.m_device.Get(), m_mmdGroundShadowPSConstantBuffer)))
		return false;
	for (const auto& mmdMat : m_mmdModel->m_materials) {
		DX11Material mat(mmdMat);
		if (!mmdMat.m_texture.empty())
			mat.m_texture = dx11Viewer.GetTexture(mmdMat.m_texture);
		if (!mmdMat.m_spTexture.empty())
			mat.m_spTexture = dx11Viewer.GetTexture(mmdMat.m_spTexture);
		if (!mmdMat.m_toonTexture.empty())
			mat.m_toonTexture = dx11Viewer.GetTexture(mmdMat.m_toonTexture);
		m_materials.emplace_back(std::move(mat));
	}
	return true;
}

void DX11Model::Update() const {
	m_mmdModel->Update();
	const size_t vtxCount = m_mmdModel->m_positions.size();
	D3D11_MAPPED_SUBRESOURCE mapRes;
	if (FAILED(m_context->Map(m_vertexBuffer.Get(), 0,
		D3D11_MAP_WRITE_DISCARD, 0, &mapRes)))
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

void DX11Model::Draw(Viewer& viewer) const {
	auto& dx11Viewer = dynamic_cast<DX11Viewer&>(viewer);
	const auto& view = viewer.m_viewMat;
	const auto& proj = viewer.m_projMat;
	const auto& dxMat = glm::mat4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.5f, 1.0f
	);
	auto world = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));
	auto wv = view * world;
	auto wvp = dxMat * proj * view * world;
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<float>(viewer.m_screenWidth);
	vp.Height = static_cast<float>(viewer.m_screenHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_context->RSSetViewports(1, &vp);
	m_context->OMSetRenderTargets(1, dx11Viewer.m_renderTargetView.GetAddressOf(), dx11Viewer.m_depthStencilView.Get());
	m_context->OMSetDepthStencilState(dx11Viewer.m_defaultDSS.Get(), 0x00);
	UINT strides[] = { sizeof(DX11Vertex) };
	UINT offsets[] = { 0 };
	m_context->IASetInputLayout(dx11Viewer.m_mmdInputLayout.Get());
	ID3D11Buffer* vbs[] = { m_vertexBuffer.Get() };
	m_context->IASetVertexBuffers(0, 1, vbs, strides, offsets);
	m_context->IASetIndexBuffer(m_indexBuffer.Get(), m_indexBufferFormat, 0);
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DX11VertexShader vsCB1{};
	vsCB1.m_wv = wv;
	vsCB1.m_wvp = wvp;
	m_context->UpdateSubresource(m_mmdVSConstantBuffer.Get(),
		0, nullptr, &vsCB1, 0, 0);
	m_context->VSSetShader(dx11Viewer.m_mmdVS.Get(), nullptr, 0);
	ID3D11Buffer* cbs1[] = { m_mmdVSConstantBuffer.Get() };
	m_context->VSSetConstantBuffers(0, 1, cbs1);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
        const auto& mat = m_materials[m_materialID];
        const auto& mmdMat = mat.m_mmdMat;
        if (mmdMat.m_diffuse.a == 0)
            continue;
        m_context->PSSetShader(dx11Viewer.m_mmdPS.Get(), nullptr, 0);
        DX11PixelShader psCB{};
        psCB.m_alpha         = mmdMat.m_diffuse.a;
        psCB.m_diffuse       = mmdMat.m_diffuse;
        psCB.m_ambient       = mmdMat.m_ambient;
        psCB.m_specular      = mmdMat.m_specular;
        psCB.m_specularPower = mmdMat.m_specularPower;
        if (mat.m_texture.m_texture) {
            psCB.m_textureModes.x = (!mat.m_texture.m_hasAlpha) ? 1 : 2;
            psCB.m_texMulFactor   = mmdMat.m_textureMulFactor;
            psCB.m_texAddFactor   = mmdMat.m_textureAddFactor;
        } else
	        psCB.m_textureModes.x = 0;
        BindOrDummyPS(m_context.Get(), 0, mat.m_texture, dx11Viewer.m_textureSampler.Get(),
            dx11Viewer.m_dummyTextureView.Get(), dx11Viewer.m_dummySampler.Get()
        );
        if (mat.m_toonTexture.m_texture) {
            psCB.m_textureModes.y    = 1;
            psCB.m_toonTexMulFactor  = mmdMat.m_toonTextureMulFactor;
            psCB.m_toonTexAddFactor  = mmdMat.m_toonTextureAddFactor;
        } else
	        psCB.m_textureModes.y = 0;
        BindOrDummyPS(m_context.Get(), 1, mat.m_toonTexture, dx11Viewer.m_toonTextureSampler.Get(),
        	dx11Viewer.m_dummyTextureView.Get(), dx11Viewer.m_dummySampler.Get()
        );
        if (mat.m_spTexture.m_texture) {
            if (mmdMat.m_spTextureMode == SphereMode::Mul)
                psCB.m_textureModes.z = 1;
            else if (mmdMat.m_spTextureMode == SphereMode::Add)
                psCB.m_textureModes.z = 2;
            else
                psCB.m_textureModes.z = 0;
            psCB.m_sphereTexMulFactor = mmdMat.m_spTextureMulFactor;
            psCB.m_sphereTexAddFactor = mmdMat.m_spTextureAddFactor;
        } else
	        psCB.m_textureModes.z = 0;
        BindOrDummyPS(m_context.Get(), 2, mat.m_spTexture, dx11Viewer.m_sphereTextureSampler.Get(),
            dx11Viewer.m_dummyTextureView.Get(), dx11Viewer.m_dummySampler.Get()
        );
        psCB.m_lightColor = viewer.m_lightColor;
        glm::vec3 lightDir = viewer.m_lightDir;
        auto viewMat3 = glm::mat3(viewer.m_viewMat);
        lightDir = viewMat3 * lightDir;
        psCB.m_lightDir = lightDir;
        m_context->UpdateSubresource(m_mmdPSConstantBuffer.Get(), 0, nullptr, &psCB, 0, 0);
        m_context->PSSetConstantBuffers(1, 1, m_mmdPSConstantBuffer.GetAddressOf());
        if (mmdMat.m_bothFace)
            m_context->RSSetState(dx11Viewer.m_mmdBothFaceRS.Get());
        else
            m_context->RSSetState(dx11Viewer.m_mmdFrontFaceRS.Get());
        m_context->OMSetBlendState(dx11Viewer.m_mmdBlendState.Get(), nullptr, 0xffffffff);
        m_context->DrawIndexed(m_vertexCount, m_beginIndex, 0);
    }
	ID3D11ShaderResourceView* views[] = { nullptr, nullptr, nullptr };
	ID3D11SamplerState* samplers[] = { nullptr, nullptr, nullptr };
	m_context->PSSetShaderResources(0, 3, views);
	m_context->PSSetSamplers(0, 3, samplers);
	m_context->IASetInputLayout(dx11Viewer.m_mmdEdgeInputLayout.Get());
	DX11EdgeVertexShader vsCB2{};
	vsCB2.m_wv = wv;
	vsCB2.m_wvp = wvp;
	vsCB2.m_screenSize = glm::vec2(static_cast<float>(viewer.m_screenWidth),
		static_cast<float>(viewer.m_screenHeight));
	m_context->UpdateSubresource(m_mmdEdgeVSConstantBuffer.Get(),
		0, nullptr, &vsCB2, 0, 0);
	m_context->VSSetShader(dx11Viewer.m_mmdEdgeVS.Get(), nullptr, 0);
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
		m_context->PSSetShader(dx11Viewer.m_mmdEdgePS.Get(), nullptr, 0);
		DX11EdgePixelShader psCB{};
		psCB.m_edgeColor = mmdMat.m_edgeColor;
		m_context->UpdateSubresource(m_mmdEdgePSConstantBuffer.Get(),
			0, nullptr, &psCB, 0, 0);
		m_context->PSSetConstantBuffers(2, 1, m_mmdEdgePSConstantBuffer.GetAddressOf());
		m_context->RSSetState(dx11Viewer.m_mmdEdgeRS.Get());
		m_context->OMSetBlendState(dx11Viewer.m_mmdEdgeBlendState.Get(), nullptr, 0xffffffff);
		m_context->DrawIndexed(m_vertexCount, m_beginIndex, 0);
	}
	m_context->IASetInputLayout(dx11Viewer.m_mmdGroundShadowInputLayout.Get());
	auto plane = glm::vec4(0, 1, 0, 0);
	auto light = -viewer.m_lightDir;
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
	DX11GroundShadowVertexShader vsCB{};
	vsCB.m_wvp = dxMat * proj * view * shadow * world;
	m_context->UpdateSubresource(m_mmdGroundShadowVSConstantBuffer.Get(),
		0, nullptr, &vsCB, 0, 0);
	m_context->VSSetShader(dx11Viewer.m_mmdGroundShadowVS.Get(), nullptr, 0);
	ID3D11Buffer* cbs[] = { m_mmdGroundShadowVSConstantBuffer.Get() };
	m_context->VSSetConstantBuffers(0, 1, cbs);
	m_context->RSSetState(dx11Viewer.m_mmdGroundShadowRS.Get());
	m_context->OMSetBlendState(dx11Viewer.m_mmdGroundShadowBlendState.Get(), nullptr, 0xffffffff);
	m_context->OMSetDepthStencilState(dx11Viewer.m_mmdGroundShadowDSS.Get(), 0x01);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		if (!mmdMat.m_groundShadow)
			continue;
		if (mat.m_mmdMat.m_diffuse.a == 0)
			continue;
		m_context->PSSetShader(dx11Viewer.m_mmdGroundShadowPS.Get(), nullptr, 0);
		DX11GroundShadowPixelShader psCB{};
		psCB.m_shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
		m_context->UpdateSubresource(m_mmdGroundShadowPSConstantBuffer.Get(), 0, nullptr, &psCB, 0, 0);
		m_context->PSSetConstantBuffers(1, 1, m_mmdGroundShadowPSConstantBuffer.GetAddressOf());
		m_context->OMSetBlendState(dx11Viewer.m_mmdEdgeBlendState.Get(), nullptr, 0xffffffff);
		m_context->DrawIndexed(m_vertexCount, m_beginIndex, 0);
	}
}

void DX11Viewer::ConfigureGlfwHints() {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

bool DX11Viewer::Setup() {
	HWND__* hwnd = glfwGetWin32Window(m_window);
	D3D_FEATURE_LEVEL featureLevel;
	constexpr D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
		featureLevels, 1, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context))) {
		glfwTerminate();
		return false;
	}
	Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
	if (FAILED(m_device.As(&dxgiDevice))) {
		glfwTerminate();
		return false;
	}
	Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
	if (FAILED(dxgiDevice->GetAdapter(&adapter))) {
		glfwTerminate();
		return false;
	}
	Microsoft::WRL::ComPtr<IDXGIFactory> factory;
	if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory), &factory))) {
		glfwTerminate();
		return false;
	}
	m_multiSampleCount = 4;
	UINT quality = 0;
	if (FAILED(m_device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, m_multiSampleCount, &quality)) || quality == 0) {
		m_multiSampleCount = 1;
		quality = 0;
	}
	m_multiSampleQuality = quality > 0 ? quality - 1 : 0;
	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferCount = 2;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = m_multiSampleCount;
	sd.SampleDesc.Quality = m_multiSampleQuality;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	if (FAILED(factory->CreateSwapChain(m_device.Get(), &sd, &m_swapChain)))
		return false;
	if (!CreateRenderTargets()) {
		glfwTerminate();
		return false;
	}
	InitDirs("shader_DX11");
	if (!CreateShaders()) {
		glfwTerminate();
		return false;
	}
	if (!CreatePipelineStates()) {
		glfwTerminate();
		return false;
	}
	if (!CreateDummyResources()) {
		glfwTerminate();
		return false;
	}
	return true;
}

bool DX11Viewer::Resize() {
	m_renderTargetView.Reset();
	m_depthStencilView.Reset();
	m_depthTex.Reset();
	if (FAILED(m_swapChain->ResizeBuffers(0, m_screenWidth, m_screenHeight, DXGI_FORMAT_UNKNOWN, 0)))
		return false;
	if (!CreateRenderTargets())
		return false;
	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<float>(m_screenWidth);
	vp.Height = static_cast<float>(m_screenHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	m_context->RSSetViewports(1, &vp);
	return true;
}

void DX11Viewer::BeginFrame() {
	m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
	m_context->ClearRenderTargetView(m_renderTargetView.Get(), m_clearColor);
	m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

bool DX11Viewer::EndFrame() {
	if (FAILED(m_swapChain->Present(0, 0)))
		return false;
	return true;
}

void DX11Viewer::AfterModelDraw(Model& model) {
	const auto& dx11Model = dynamic_cast<DX11Model&>(model);
	Microsoft::WRL::ComPtr<ID3D11CommandList> cmd;
	if (SUCCEEDED(dx11Model.m_context->FinishCommandList(FALSE, &cmd)) && cmd)
		m_context->ExecuteCommandList(cmd.Get(), FALSE);
}

std::unique_ptr<Model> DX11Viewer::CreateModel() const {
	return std::make_unique<DX11Model>();
}

DX11Texture DX11Viewer::GetTexture(const std::filesystem::path& texturePath) {
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
	const HRESULT hr = m_device->CreateTexture2D(&tex2dDesc, &initData, &tex2d);
	stbi_image_free(image);
	if (FAILED(hr))
		return {};
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex2dRV;
	if (FAILED(m_device->CreateShaderResourceView(tex2d.Get(), nullptr, &tex2dRV)))
		return {};
	DX11Texture tex;
	tex.m_texture = tex2d;
	tex.m_textureView = tex2dRV;
	tex.m_hasAlpha = hasAlpha;
	m_textures[texturePath] = tex;
	return m_textures[texturePath];
}

bool DX11Viewer::CreateShaders() {
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	Microsoft::WRL::ComPtr<ID3DBlob>
			mmdVSBlob, mmdPSBlob,
			mmdEdgeVSBlob, mmdEdgePSBlob,
			mmdGroundShadowVSBlob, mmdGroundShadowPSBlob;
	if (!CompileShader(m_shaderDir / "mmd.hlsl", "VSMain", "vs_5_0", mmdVSBlob))
		return false;
	if (!CompileShader(m_shaderDir / "mmd.hlsl", "PSMain", "ps_5_0", mmdPSBlob))
		return false;
	if (!CompileShader(m_shaderDir / "mmd_edge.hlsl", "VSMain", "vs_5_0", mmdEdgeVSBlob))
		return false;
	if (!CompileShader(m_shaderDir / "mmd_edge.hlsl", "PSMain", "ps_5_0", mmdEdgePSBlob))
		return false;
	if (!CompileShader(m_shaderDir / "mmd_ground_shadow.hlsl", "VSMain", "vs_5_0", mmdGroundShadowVSBlob))
		return false;
	if (!CompileShader(m_shaderDir / "mmd_ground_shadow.hlsl", "PSMain", "ps_5_0", mmdGroundShadowPSBlob))
		return false;
	if (FAILED(m_device->CreateVertexShader(
		mmdVSBlob->GetBufferPointer(), mmdVSBlob->GetBufferSize(),
		nullptr, &m_mmdVS)))
		return false;
	if (FAILED(m_device->CreatePixelShader(
		mmdPSBlob->GetBufferPointer(), mmdPSBlob->GetBufferSize(),
		nullptr, &m_mmdPS)))
		return false;
	if (FAILED(m_device->CreateVertexShader(
		mmdEdgeVSBlob->GetBufferPointer(), mmdEdgeVSBlob->GetBufferSize(),
		nullptr, &m_mmdEdgeVS)))
		return false;
	if (FAILED(m_device->CreatePixelShader(
		mmdEdgePSBlob->GetBufferPointer(), mmdEdgePSBlob->GetBufferSize(),
		nullptr, &m_mmdEdgePS)))
		return false;
	if (FAILED(m_device->CreateVertexShader(
		mmdGroundShadowVSBlob->GetBufferPointer(), mmdGroundShadowVSBlob->GetBufferSize(),
		nullptr, &m_mmdGroundShadowVS)))
		return false;
	if (FAILED(m_device->CreatePixelShader(
		mmdGroundShadowPSBlob->GetBufferPointer(), mmdGroundShadowPSBlob->GetBufferSize(),
		nullptr, &m_mmdGroundShadowPS)))
		return false;
	constexpr D3D11_INPUT_ELEMENT_DESC mmdInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(m_device->CreateInputLayout(
		mmdInputElementDesc, 3,
		mmdVSBlob->GetBufferPointer(), mmdVSBlob->GetBufferSize(),
		&m_mmdInputLayout)))
		return false;
	constexpr D3D11_INPUT_ELEMENT_DESC mmdEdgeInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(m_device->CreateInputLayout(
		mmdEdgeInputElementDesc, 2,
		mmdEdgeVSBlob->GetBufferPointer(), mmdEdgeVSBlob->GetBufferSize(),
		&m_mmdEdgeInputLayout)))
		return false;
	constexpr D3D11_INPUT_ELEMENT_DESC mmdGroundShadowInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(m_device->CreateInputLayout(
		mmdGroundShadowInputElementDesc, 1,
		mmdGroundShadowVSBlob->GetBufferPointer(), mmdGroundShadowVSBlob->GetBufferSize(),
		&m_mmdGroundShadowInputLayout)))
		return false;
	return true;
}

bool DX11Viewer::CreateRenderTargets() {
	m_renderTargetView.Reset();
	m_depthTex.Reset();
	m_depthStencilView.Reset();
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	if (FAILED(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
		return false;
	if (FAILED(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView)))
		return false;
	D3D11_TEXTURE2D_DESC dsDesc{};
	dsDesc.Width = static_cast<UINT>(m_screenWidth);
	dsDesc.Height = static_cast<UINT>(m_screenHeight);
	dsDesc.MipLevels = 1;
	dsDesc.ArraySize = 1;
	dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsDesc.SampleDesc.Count = m_multiSampleCount;
	dsDesc.SampleDesc.Quality = m_multiSampleQuality;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	if (FAILED(m_device->CreateTexture2D(&dsDesc, nullptr, &m_depthTex)))
		return false;
	if (FAILED(m_device->CreateDepthStencilView(m_depthTex.Get(), nullptr, &m_depthStencilView)))
		return false;
	return true;
}

bool DX11Viewer::CreatePipelineStates() {
	auto wrapLinear = MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP,
	D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
	if (FAILED(m_device->CreateSamplerState(&wrapLinear, &m_textureSampler)))
		return false;
	m_sphereTextureSampler = m_textureSampler;
	m_dummySampler = m_textureSampler;
	auto clampLinear = MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP);
	if (FAILED(m_device->CreateSamplerState(&clampLinear, &m_toonTextureSampler)))
		return false;
	auto blend = MakeAlphaBlendDesc();
	if (FAILED(m_device->CreateBlendState(&blend, &m_mmdBlendState)))
		return false;
	m_mmdEdgeBlendState = m_mmdBlendState;
	m_mmdGroundShadowBlendState = m_mmdBlendState;
	auto frontRsDesc = MakeRasterDesc(D3D11_CULL_BACK, true);
	if (FAILED(m_device->CreateRasterizerState(&frontRsDesc, &m_mmdFrontFaceRS)))
		return false;
	auto bothRsDesc = MakeRasterDesc(D3D11_CULL_NONE, true);
	if (FAILED(m_device->CreateRasterizerState(&bothRsDesc, &m_mmdBothFaceRS)))
		return false;
	auto edgeRsDesc = MakeRasterDesc(D3D11_CULL_FRONT, true);
	edgeRsDesc.DepthClipEnable = FALSE;
	if (FAILED(m_device->CreateRasterizerState(&edgeRsDesc, &m_mmdEdgeRS)))
		return false;
	auto groundShadowRsDesc = MakeRasterDesc(D3D11_CULL_NONE, true, -1, -1.0f, -1.0f);
	groundShadowRsDesc.DepthClipEnable = FALSE;
	if (FAILED(m_device->CreateRasterizerState(&groundShadowRsDesc, &m_mmdGroundShadowRS)))
		return false;
	auto gsDSS = MakeGroundShadowDepthStencilDesc();
	if (FAILED(m_device->CreateDepthStencilState(&gsDSS, &m_mmdGroundShadowDSS)))
		return false;
	auto defDSS = MakeDefaultDepthStencilDesc();
	if (FAILED(m_device->CreateDepthStencilState(&defDSS, &m_defaultDSS)))
		return false;
	return true;
}

bool DX11Viewer::CreateDummyResources() {
	D3D11_TEXTURE2D_DESC tex2dDesc{};
	tex2dDesc.Width = 1;
	tex2dDesc.Height = 1;
	tex2dDesc.MipLevels = 1;
	tex2dDesc.ArraySize = 1;
	tex2dDesc.SampleDesc.Count = 1;
	tex2dDesc.SampleDesc.Quality = 0;
	tex2dDesc.Usage = D3D11_USAGE_DEFAULT;
	tex2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tex2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	if (FAILED(m_device->CreateTexture2D(&tex2dDesc, nullptr, &m_dummyTexture)))
		return false;
	if (FAILED(m_device->CreateShaderResourceView(m_dummyTexture.Get(), nullptr, &m_dummyTextureView)))
		return false;
	return true;
}
