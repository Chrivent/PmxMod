#include "DX11Viewer.h"

#include "../src/Model.h"

#include "../external/stb_image.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <d3dcompiler.h>

D3D11_SAMPLER_DESC Sampler(const D3D11_FILTER f, const D3D11_TEXTURE_ADDRESS_MODE addr) {
	CD3D11_SAMPLER_DESC d(D3D11_DEFAULT);
	d.Filter = f;
	d.AddressU = d.AddressV = d.AddressW = addr;
	return d;
}

D3D11_RASTERIZER_DESC Raster(const D3D11_CULL_MODE cull, const bool frontCCW) {
	CD3D11_RASTERIZER_DESC d(D3D11_DEFAULT);
	d.CullMode = cull;
	d.FrontCounterClockwise = frontCCW;
	d.MultisampleEnable = TRUE;
	return d;
}

D3D11_BLEND_DESC AlphaBlend() {
	CD3D11_BLEND_DESC d(D3D11_DEFAULT);
	d.RenderTarget[0].BlendEnable = TRUE;
	d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	return d;
}

D3D11_DEPTH_STENCIL_DESC MakeDefaultDepthStencilDesc() {
	CD3D11_DEPTH_STENCIL_DESC d(CD3D11_DEFAULT{});
	d.DepthEnable = TRUE;
	d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	d.DepthFunc = D3D11_COMPARISON_LESS;
	d.StencilEnable = FALSE;
	return d;
}

D3D11_DEPTH_STENCIL_DESC MakeGroundShadowDepthStencilDesc() {
	CD3D11_DEPTH_STENCIL_DESC d(CD3D11_DEFAULT{});
	d.DepthEnable = TRUE;
	d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	d.DepthFunc = D3D11_COMPARISON_LESS;
	d.StencilEnable = TRUE;
	d.StencilReadMask  = 0x01;
	d.StencilWriteMask = 0xFF;
	d.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
	d.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	d.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	d.BackFace = d.FrontFace;
	return d;
}

template<class T>
HRESULT CreateBuffer(ID3D11Device* device, Microsoft::WRL::ComPtr<ID3D11Buffer>& out) {
	const UINT bytes = static_cast<UINT>(sizeof(T) + 15u & ~15u);
	const CD3D11_BUFFER_DESC desc(bytes, D3D11_BIND_CONSTANT_BUFFER);
	return device->CreateBuffer(&desc, nullptr, out.GetAddressOf());
}

void BindTexture(ID3D11DeviceContext* context, ID3D11ShaderResourceView* dummySRV, ID3D11SamplerState* dummySampler,
	const UINT slot, const DX11Texture& tex, ID3D11SamplerState* sampler, const int modeIfPresent, int& outMode,
	glm::vec4& outMul, glm::vec4& outAdd, const glm::vec4& mulIn, const glm::vec4& addIn) {
	if (tex.m_texture) {
		outMode = modeIfPresent;
		outMul  = mulIn;
		outAdd  = addIn;
	} else
		outMode = 0;
	ID3D11ShaderResourceView* views = tex.m_texture ? tex.m_textureView.Get() : dummySRV;
	ID3D11SamplerState* samplers = tex.m_texture ? sampler : dummySampler;
	context->PSSetShaderResources(slot, 1, &views);
	context->PSSetSamplers(slot, 1, &samplers);
}

DX11Material::DX11Material(const Material& mat)
	: m_mat(mat) {
}

bool DX11Instance::Setup(Viewer& viewer) {
	m_viewer = &dynamic_cast<DX11Viewer&>(viewer);
	D3D11_BUFFER_DESC vBufDesc = {};
	vBufDesc.Usage = D3D11_USAGE_DYNAMIC;
	vBufDesc.ByteWidth = static_cast<UINT>(sizeof(DX11Vertex) * m_model->m_positions.size());
	vBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(m_viewer->m_device->CreateBuffer(&vBufDesc, nullptr, &m_vertexBuffer)))
		return false;
	D3D11_BUFFER_DESC iBufDesc = {};
	iBufDesc.Usage = D3D11_USAGE_IMMUTABLE;
	iBufDesc.ByteWidth = static_cast<UINT>(m_model->m_indexElementSize * m_model->m_indexCount);
	iBufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	iBufDesc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = &m_model->m_indices[0];
	if (FAILED(m_viewer->m_device->CreateBuffer(&iBufDesc, &initData, &m_indexBuffer)))
		return false;
	if (1 == m_model->m_indexElementSize)
		m_indexBufferFormat = DXGI_FORMAT_R8_UINT;
	else if (2 == m_model->m_indexElementSize)
		m_indexBufferFormat = DXGI_FORMAT_R16_UINT;
	else if (4 == m_model->m_indexElementSize)
		m_indexBufferFormat = DXGI_FORMAT_R32_UINT;
	else
		return false;
	if (FAILED(CreateBuffer<DX11VertexShader>(m_viewer->m_device.Get(), m_vsConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11PixelShader>(m_viewer->m_device.Get(), m_psConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11EdgeVertexShader>(m_viewer->m_device.Get(), m_edgeVsConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11EdgeSizeVertexShader>(m_viewer->m_device.Get(), m_edgeSizeVsConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11EdgePixelShader>(m_viewer->m_device.Get(), m_edgePsConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11GroundShadowVertexShader>(m_viewer->m_device.Get(), m_gsVsConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11GroundShadowPixelShader>(m_viewer->m_device.Get(), m_gsPsConstantBuffer)))
		return false;
	for (const auto& mat : m_model->m_materials) {
		DX11Material m(mat);
		if (!mat.m_texture.empty())
			m.m_texture = m_viewer->GetTexture(mat.m_texture);
		if (!mat.m_spTexture.empty())
			m.m_spTexture = m_viewer->GetTexture(mat.m_spTexture);
		if (!mat.m_toonTexture.empty())
			m.m_toonTexture = m_viewer->GetTexture(mat.m_toonTexture);
		m_materials.emplace_back(std::move(m));
	}
	return true;
}

void DX11Instance::Update() const {
	m_model->Update();
	const size_t vtxCount = m_model->m_positions.size();
	D3D11_MAPPED_SUBRESOURCE mapRes;
	if (FAILED(m_viewer->m_context->Map(m_vertexBuffer.Get(), 0,
		D3D11_MAP_WRITE_DISCARD, 0, &mapRes)))
		return;
	const auto vertices = static_cast<DX11Vertex*>(mapRes.pData);
	const glm::vec3* positions = m_model->m_updatePositions.data();
	const glm::vec3* normals = m_model->m_updateNormals.data();
	const glm::vec2* uvs = m_model->m_updateUVs.data();
	for (size_t i = 0; i < vtxCount; i++) {
		vertices[i].m_position = positions[i];
		vertices[i].m_normal = normals[i];
		vertices[i].m_uv = uvs[i];
	}
	m_viewer->m_context->Unmap(m_vertexBuffer.Get(), 0);
}

void DX11Instance::Draw() const {
	const auto& view = m_viewer->m_viewMat;
	const auto& proj = m_viewer->m_projMat;
	const auto& dxMat = glm::mat4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.5f, 1.0f
	);
	auto world = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));
	auto wv = view * world;
	auto wvp = dxMat * proj * view * world;
	m_viewer->m_context->OMSetDepthStencilState(m_viewer->m_defaultDss.Get(), 0x00);
	UINT stride = sizeof(DX11Vertex);
	UINT offset = 0;
	m_viewer->m_context->IASetInputLayout(m_viewer->m_inputLayout.Get());
	m_viewer->m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	m_viewer->m_context->IASetIndexBuffer(m_indexBuffer.Get(), m_indexBufferFormat, 0);
	m_viewer->m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DX11VertexShader vsCB1{};
	vsCB1.m_wv = wv;
	vsCB1.m_wvp = wvp;
	m_viewer->m_context->UpdateSubresource(m_vsConstantBuffer.Get(),
		0, nullptr, &vsCB1, 0, 0);
	m_viewer->m_context->VSSetShader(m_viewer->m_vs.Get(), nullptr, 0);
	m_viewer->m_context->PSSetShader(m_viewer->m_ps.Get(), nullptr, 0);
	m_viewer->m_context->VSSetConstantBuffers(0, 1, m_vsConstantBuffer.GetAddressOf());
	for (const auto& [m_beginIndex, m_indexCount, m_materialID] : m_model->m_subMeshes) {
        const auto& m = m_materials[m_materialID];
        const auto& mat = m.m_mat;
        if (mat.m_diffuse.a == 0)
            continue;
        DX11PixelShader psCB{};
        psCB.m_alpha         = mat.m_diffuse.a;
        psCB.m_diffuse       = mat.m_diffuse;
        psCB.m_ambient       = mat.m_ambient;
        psCB.m_specular      = mat.m_specular;
        psCB.m_specularPower = mat.m_specularPower;
		int baseMode = 0;
		if (m.m_texture.m_texture)
			baseMode = !m.m_texture.m_hasAlpha ? 1 : 2;
		BindTexture(m_viewer->m_context.Get(), m_viewer->m_dummyTextureView.Get(), m_viewer->m_textureSampler.Get(),
			0, m.m_texture, m_viewer->m_textureSampler.Get(), baseMode, psCB.m_textureModes.x,
			psCB.m_texMulFactor, psCB.m_texAddFactor, mat.m_textureMulFactor, mat.m_textureAddFactor);
		BindTexture(m_viewer->m_context.Get(), m_viewer->m_dummyTextureView.Get(), m_viewer->m_textureSampler.Get(),
			1, m.m_toonTexture, m_viewer->m_toonTextureSampler.Get(), 1, psCB.m_textureModes.y,
			psCB.m_toonTexMulFactor, psCB.m_toonTexAddFactor, mat.m_toonTextureMulFactor, mat.m_toonTextureAddFactor);
		int spMode = 0;
		if (m.m_spTexture.m_texture) {
			if (mat.m_spTextureMode == SphereMode::Mul)
				spMode = 1;
			else if (mat.m_spTextureMode == SphereMode::Add)
				spMode = 2;
		}
		BindTexture(m_viewer->m_context.Get(), m_viewer->m_dummyTextureView.Get(), m_viewer->m_textureSampler.Get(),
			2, m.m_spTexture, m_viewer->m_textureSampler.Get(), spMode, psCB.m_textureModes.z,
			psCB.m_sphereTexMulFactor, psCB.m_sphereTexAddFactor, mat.m_spTextureMulFactor, mat.m_spTextureAddFactor);
        psCB.m_lightColor = m_viewer->m_lightColor;
        psCB.m_lightDir = glm::mat3(m_viewer->m_viewMat) * m_viewer->m_lightDir;
        m_viewer->m_context->UpdateSubresource(m_psConstantBuffer.Get(), 0, nullptr, &psCB, 0, 0);
        m_viewer->m_context->PSSetConstantBuffers(1, 1, m_psConstantBuffer.GetAddressOf());
        if (mat.m_bothFace)
            m_viewer->m_context->RSSetState(m_viewer->m_bothFaceRs.Get());
        else
            m_viewer->m_context->RSSetState(m_viewer->m_frontFaceRs.Get());
        m_viewer->m_context->DrawIndexed(m_indexCount, m_beginIndex, 0);
    }
	m_viewer->m_context->IASetInputLayout(m_viewer->m_edgeInputLayout.Get());
	DX11EdgeVertexShader vsCB2{};
	vsCB2.m_wv = wv;
	vsCB2.m_wvp = wvp;
	vsCB2.m_screenSize = glm::vec2(static_cast<float>(m_viewer->m_screenWidth),
		static_cast<float>(m_viewer->m_screenHeight));
	m_viewer->m_context->UpdateSubresource(m_edgeVsConstantBuffer.Get(),
		0, nullptr, &vsCB2, 0, 0);
	m_viewer->m_context->VSSetShader(m_viewer->m_edgeVs.Get(), nullptr, 0);
	m_viewer->m_context->PSSetShader(m_viewer->m_edgePs.Get(), nullptr, 0);
	m_viewer->m_context->VSSetConstantBuffers(0, 1, m_edgeVsConstantBuffer.GetAddressOf());
	for (const auto& [m_beginIndex, m_indexCount, m_materialID] : m_model->m_subMeshes) {
		const auto& m = m_materials[m_materialID];
		const auto& mat = m.m_mat;
		if (!mat.m_edgeFlag)
			continue;
		if (mat.m_diffuse.a == 0)
			continue;
		DX11EdgeSizeVertexShader vsCB{};
		vsCB.m_edgeSize = mat.m_edgeSize;
		m_viewer->m_context->UpdateSubresource(m_edgeSizeVsConstantBuffer.Get(),
			0, nullptr, &vsCB, 0, 0);
		m_viewer->m_context->VSSetConstantBuffers(1, 1, m_edgeSizeVsConstantBuffer.GetAddressOf());
		DX11EdgePixelShader psCB{};
		psCB.m_edgeColor = mat.m_edgeColor;
		m_viewer->m_context->UpdateSubresource(m_edgePsConstantBuffer.Get(),
			0, nullptr, &psCB, 0, 0);
		m_viewer->m_context->PSSetConstantBuffers(2, 1, m_edgePsConstantBuffer.GetAddressOf());
		m_viewer->m_context->RSSetState(m_viewer->m_edgeRs.Get());
		m_viewer->m_context->DrawIndexed(m_indexCount, m_beginIndex, 0);
	}
	m_viewer->m_context->IASetInputLayout(m_viewer->m_gsInputLayout.Get());
	glm::vec4 plane(0.f, 1.f, 0.f, 0.f);
	glm::vec4 light(-glm::normalize(m_viewer->m_lightDir), 0.f);
	glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.f) - glm::outerProduct(light, plane);
	DX11GroundShadowVertexShader vsCB3{};
	vsCB3.m_wvp = dxMat * proj * view * shadow * world;
	m_viewer->m_context->UpdateSubresource(m_gsVsConstantBuffer.Get(),
		0, nullptr, &vsCB3, 0, 0);
	m_viewer->m_context->VSSetShader(m_viewer->m_gsVs.Get(), nullptr, 0);
	m_viewer->m_context->PSSetShader(m_viewer->m_gsPs.Get(), nullptr, 0);
	m_viewer->m_context->VSSetConstantBuffers(0, 1, m_gsVsConstantBuffer.GetAddressOf());
	m_viewer->m_context->RSSetState(m_viewer->m_gsRs.Get());
	m_viewer->m_context->OMSetDepthStencilState(m_viewer->m_gsDss.Get(), 0x01);
	for (const auto& [m_beginIndex, m_indexCount, m_materialID] : m_model->m_subMeshes) {
		const auto& m = m_materials[m_materialID];
		const auto& mat = m.m_mat;
		if (!mat.m_groundShadow)
			continue;
		if (mat.m_diffuse.a == 0)
			continue;
		DX11GroundShadowPixelShader psCB{};
		psCB.m_shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
		m_viewer->m_context->UpdateSubresource(m_gsPsConstantBuffer.Get(), 0, nullptr, &psCB, 0, 0);
		m_viewer->m_context->PSSetConstantBuffers(1, 1, m_gsPsConstantBuffer.GetAddressOf());
		m_viewer->m_context->DrawIndexed(m_indexCount, m_beginIndex, 0);
	}
}

void DX11Viewer::ConfigureGlfwHints() {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

bool DX11Viewer::Setup() {
	HWND__* hwnd = glfwGetWin32Window(m_window);
	constexpr D3D_FEATURE_LEVEL featureLevels = D3D_FEATURE_LEVEL_11_0;
	if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
		&featureLevels, 1, D3D11_SDK_VERSION, &m_device, nullptr, &m_context)))
		return false;
	Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
	if (FAILED(m_device.As(&dxgiDevice)))
		return false;
	Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
	if (FAILED(dxgiDevice->GetAdapter(&adapter)))
		return false;
	Microsoft::WRL::ComPtr<IDXGIFactory> factory;
	if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory), &factory)))
		return false;
	m_multiSampleCount = 4;
	UINT quality = 0;
	if (FAILED(m_device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, m_multiSampleCount, &quality)) || quality == 0) {
		m_multiSampleCount = 1;
		quality = 0;
	}
	m_multiSampleQuality = quality > 0 ? quality - 1 : 0;
	DXGI_SWAP_CHAIN_DESC d{};
	d.BufferCount = 2;
	d.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	d.OutputWindow = hwnd;
	d.SampleDesc.Count = m_multiSampleCount;
	d.SampleDesc.Quality = m_multiSampleQuality;
	d.Windowed = TRUE;
	if (FAILED(factory->CreateSwapChain(m_device.Get(), &d, &m_swapChain)))
		return false;
	if (!CreateRenderTargets())
		return false;
	InitDirs("shader_DX11");
	if (!CreateShaders())
		return false;
	if (!CreatePipelineStates())
		return false;
	if (!CreateDummyResources())
		return false;
	UpdateViewport();
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
	UpdateViewport();
	return true;
}

void DX11Viewer::BeginFrame() {
	m_context->ClearRenderTargetView(m_renderTargetView.Get(), m_clearColor);
	m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
	m_context->OMSetBlendState(m_blendState.Get(), nullptr, 0xffffffff);
}

bool DX11Viewer::EndFrame() {
	if (FAILED(m_swapChain->Present(0, 0)))
		return false;
	return true;
}

std::unique_ptr<Instance> DX11Viewer::CreateInstance() const {
	return std::make_unique<DX11Instance>();
}

DX11Texture DX11Viewer::GetTexture(const std::filesystem::path& texturePath) {
	const auto it = m_textures.find(texturePath);
	if (it != m_textures.end())
		return it->second;
	int x = 0, y = 0, comp = 0;
	stbi_uc* image = LoadImageRGBA(texturePath, x, y, comp);
	if (!image)
		return {};
	D3D11_TEXTURE2D_DESC d = {};
	d.Width = x;
	d.Height = y;
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.SampleDesc.Count = 1;
	d.SampleDesc.Quality = 0;
	d.Usage = D3D11_USAGE_DEFAULT;
	d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	d.CPUAccessFlags = 0;
	d.MiscFlags = 0;
	const bool hasAlpha = comp == 4;
	d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = image;
	initData.SysMemPitch = 4 * x;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2d;
	const HRESULT hr = m_device->CreateTexture2D(&d, &initData, &tex2d);
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

void DX11Viewer::UpdateViewport() const {
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<float>(m_screenWidth);
	vp.Height = static_cast<float>(m_screenHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_context->RSSetViewports(1, &vp);
}

bool DX11Viewer::MakeVS(const std::filesystem::path& f, const char* entry,
	Microsoft::WRL::ComPtr<ID3D11VertexShader>& outVS, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob) const {
	if (FAILED(D3DCompileFromFile(f.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entry, "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &outBlob, nullptr)))
		return false;
	if (FAILED(m_device->CreateVertexShader(outBlob->GetBufferPointer(), outBlob->GetBufferSize(), nullptr, &outVS)))
		return false;
	return true;
}

bool DX11Viewer::MakePS(const std::filesystem::path& f, const char* entry, Microsoft::WRL::ComPtr<ID3D11PixelShader>& outPS) const {
	Microsoft::WRL::ComPtr<ID3DBlob> blob;
	if (FAILED(D3DCompileFromFile(f.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entry, "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, nullptr)))
		return false;
	if (FAILED(m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &outPS)))
		return false;
	return true;
}

bool DX11Viewer::CreateShaders() {
	Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, edgeVsBlob, gsVsBlob;
	if (!MakeVS(m_shaderDir / "mmd.hlsl", "VSMain", m_vs, vsBlob))
		return false;
	if (!MakeVS(m_shaderDir / "mmd_edge.hlsl", "VSMain", m_edgeVs, edgeVsBlob))
		return false;
	if (!MakeVS(m_shaderDir / "mmd_ground_shadow.hlsl", "VSMain", m_gsVs, gsVsBlob))
		return false;
	if (!MakePS(m_shaderDir / "mmd.hlsl", "PSMain", m_ps))
		return false;
	if (!MakePS(m_shaderDir / "mmd_edge.hlsl", "PSMain", m_edgePs))
		return false;
	if (!MakePS(m_shaderDir / "mmd_ground_shadow.hlsl", "PSMain", m_gsPs))
		return false;
	constexpr D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(m_device->CreateInputLayout(
		inputElementDesc, 3,
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
		&m_inputLayout)))
		return false;
	constexpr D3D11_INPUT_ELEMENT_DESC edgeInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(m_device->CreateInputLayout(
		edgeInputElementDesc, 2,
		edgeVsBlob->GetBufferPointer(), edgeVsBlob->GetBufferSize(),
		&m_edgeInputLayout)))
		return false;
	constexpr D3D11_INPUT_ELEMENT_DESC gsInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(m_device->CreateInputLayout(
		gsInputElementDesc, 1,
		gsVsBlob->GetBufferPointer(), gsVsBlob->GetBufferSize(),
		&m_gsInputLayout)))
		return false;
	return true;
}

bool DX11Viewer::CreateRenderTargets() {
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	if (FAILED(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
		return false;
	if (FAILED(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView)))
		return false;
	D3D11_TEXTURE2D_DESC d{};
	d.Width = static_cast<UINT>(m_screenWidth);
	d.Height = static_cast<UINT>(m_screenHeight);
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d.SampleDesc.Count = m_multiSampleCount;
	d.SampleDesc.Quality = m_multiSampleQuality;
	d.Usage = D3D11_USAGE_DEFAULT;
	d.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	if (FAILED(m_device->CreateTexture2D(&d, nullptr, &m_depthTex)))
		return false;
	if (FAILED(m_device->CreateDepthStencilView(m_depthTex.Get(), nullptr, &m_depthStencilView)))
		return false;
	return true;
}

bool DX11Viewer::CreatePipelineStates() {
	auto wrapLinear = Sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	if (FAILED(m_device->CreateSamplerState(&wrapLinear, &m_textureSampler)))
		return false;
	auto clampLinear = Sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
	if (FAILED(m_device->CreateSamplerState(&clampLinear, &m_toonTextureSampler)))
		return false;
	auto blend = AlphaBlend();
	if (FAILED(m_device->CreateBlendState(&blend, &m_blendState)))
		return false;
	auto frontRsDesc = Raster(D3D11_CULL_BACK, true);
	if (FAILED(m_device->CreateRasterizerState(&frontRsDesc, &m_frontFaceRs)))
		return false;
	auto bothRsDesc = Raster(D3D11_CULL_NONE, true);
	if (FAILED(m_device->CreateRasterizerState(&bothRsDesc, &m_bothFaceRs)))
		return false;
	auto edgeRsDesc = Raster(D3D11_CULL_FRONT, true);
	edgeRsDesc.DepthClipEnable = FALSE;
	if (FAILED(m_device->CreateRasterizerState(&edgeRsDesc, &m_edgeRs)))
		return false;
	auto gsRsDesc = Raster(D3D11_CULL_NONE, true);
	gsRsDesc.DepthClipEnable = FALSE;
	gsRsDesc.DepthBias = -1;
	gsRsDesc.SlopeScaledDepthBias = -1.0f;
	gsRsDesc.DepthBiasClamp = -1.0f;
	if (FAILED(m_device->CreateRasterizerState(&gsRsDesc, &m_gsRs)))
		return false;
	auto gsDSS = MakeGroundShadowDepthStencilDesc();
	if (FAILED(m_device->CreateDepthStencilState(&gsDSS, &m_gsDss)))
		return false;
	auto defDSS = MakeDefaultDepthStencilDesc();
	if (FAILED(m_device->CreateDepthStencilState(&defDSS, &m_defaultDss)))
		return false;
	return true;
}

bool DX11Viewer::CreateDummyResources() {
	D3D11_TEXTURE2D_DESC d{};
	d.Width = 1;
	d.Height = 1;
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.SampleDesc.Count = 1;
	d.SampleDesc.Quality = 0;
	d.Usage = D3D11_USAGE_DEFAULT;
	d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	if (FAILED(m_device->CreateTexture2D(&d, nullptr, &m_dummyTexture)))
		return false;
	if (FAILED(m_device->CreateShaderResourceView(m_dummyTexture.Get(), nullptr, &m_dummyTextureView)))
		return false;
	return true;
}
