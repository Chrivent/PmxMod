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

#include <d3dcompiler.h>

DX11Material::DX11Material(const MMDMaterial& mat)
	: m_mmdMat(mat) {
}

bool DX11Model::Setup(Viewer& viewer) {
	auto& dx11Viewer = static_cast<DX11Viewer&>(viewer);
	if (FAILED(dx11Viewer.m_device->CreateDeferredContext(0, &m_context)))
		return false;

	// Setup vertex buffer
	D3D11_BUFFER_DESC vBufDesc = {};
	vBufDesc.Usage = D3D11_USAGE_DYNAMIC;
	vBufDesc.ByteWidth = static_cast<UINT>(sizeof(DX11Vertex) * m_mmdModel->m_positions.size());
	vBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(dx11Viewer.m_device->CreateBuffer(&vBufDesc, nullptr, &m_vertexBuffer)))
		return false;

	// Setup index buffer;
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

	// Setup mmd vertex shader constant buffer (VSData)
	D3D11_BUFFER_DESC vsBufDesc = {};
	vsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	vsBufDesc.ByteWidth = sizeof(DX11VertexShader);
	vsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	vsBufDesc.CPUAccessFlags = 0;
	if (FAILED(dx11Viewer.m_device->CreateBuffer(&vsBufDesc, nullptr, &m_mmdVSConstantBuffer)))
		return false;

	// Setup mmd pixel shader constant buffer (PSData)
	D3D11_BUFFER_DESC psBufDesc = {};
	psBufDesc.Usage = D3D11_USAGE_DEFAULT;
	psBufDesc.ByteWidth = sizeof(DX11PixelShader);
	psBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	psBufDesc.CPUAccessFlags = 0;
	if (FAILED(dx11Viewer.m_device->CreateBuffer(&psBufDesc, nullptr, &m_mmdPSConstantBuffer)))
		return false;

	// Setup mmd edge vertex shader constant buffer (VSData)
	D3D11_BUFFER_DESC evsBufDesc1 = {};
	evsBufDesc1.Usage = D3D11_USAGE_DEFAULT;
	evsBufDesc1.ByteWidth = sizeof(DX11EdgeVertexShader);
	evsBufDesc1.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	evsBufDesc1.CPUAccessFlags = 0;
	if (FAILED(dx11Viewer.m_device->CreateBuffer(&evsBufDesc1, nullptr, &m_mmdEdgeVSConstantBuffer)))
		return false;

	// Setup mmd edge vertex shader constant buffer (VSEdgeData)
	D3D11_BUFFER_DESC evsBufDesc2 = {};
	evsBufDesc2.Usage = D3D11_USAGE_DEFAULT;
	evsBufDesc2.ByteWidth = sizeof(DX11EdgeSizeVertexShader);
	evsBufDesc2.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	evsBufDesc2.CPUAccessFlags = 0;
	if (FAILED(dx11Viewer.m_device->CreateBuffer(&evsBufDesc2, nullptr, &m_mmdEdgeSizeVSConstantBuffer)))
		return false;

	// Setup mmd edge pixel shader constant buffer (PSData)
	D3D11_BUFFER_DESC epsBufDesc = {};
	epsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	epsBufDesc.ByteWidth = sizeof(DX11EdgePixelShader);
	epsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	epsBufDesc.CPUAccessFlags = 0;
	if (FAILED(dx11Viewer.m_device->CreateBuffer(&epsBufDesc, nullptr, &m_mmdEdgePSConstantBuffer)))
		return false;

	// Setup mmd ground shadow vertex shader constant buffer (VSData)
	D3D11_BUFFER_DESC gvsBufDesc = {};
	gvsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	gvsBufDesc.ByteWidth = sizeof(DX11GroundShadowVertexShader);
	gvsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	gvsBufDesc.CPUAccessFlags = 0;
	if (FAILED(dx11Viewer.m_device->CreateBuffer(&gvsBufDesc, nullptr, &m_mmdGroundShadowVSConstantBuffer)))
		return false;

	// Setup mmd ground shadow pixel shader constant buffer (PSData)
	D3D11_BUFFER_DESC gpsBufDesc = {};
	gpsBufDesc.Usage = D3D11_USAGE_DEFAULT;
	gpsBufDesc.ByteWidth = sizeof(DX11GroundShadowPixelShader);
	gpsBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	gpsBufDesc.CPUAccessFlags = 0;
	if (FAILED(dx11Viewer.m_device->CreateBuffer(&gpsBufDesc, nullptr, &m_mmdGroundShadowPSConstantBuffer)))
		return false;

	// Setup materials
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
	auto& dx11Viewer = static_cast<DX11Viewer&>(viewer);
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

	// Set viewport
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<float>(viewer.m_screenWidth);
	vp.Height = static_cast<float>(viewer.m_screenHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_context->RSSetViewports(1, &vp);
	ID3D11RenderTargetView* rtvs[] = { dx11Viewer.m_renderTargetView.Get() };
	m_context->OMSetRenderTargets(1, rtvs, dx11Viewer.m_depthStencilView.Get());
	m_context->OMSetDepthStencilState(dx11Viewer.m_defaultDSS.Get(), 0x00);

	// Setup input assembler
	UINT strides[] = { sizeof(DX11Vertex) };
	UINT offsets[] = { 0 };
	m_context->IASetInputLayout(dx11Viewer.m_mmdInputLayout.Get());
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
	m_context->VSSetShader(dx11Viewer.m_mmdVS.Get(), nullptr, 0);
	ID3D11Buffer* cbs1[] = { m_mmdVSConstantBuffer.Get() };
	m_context->VSSetConstantBuffers(0, 1, cbs1);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& mat = m_materials[m_materialID];
		const auto& mmdMat = mat.m_mmdMat;
		if (mat.m_mmdMat.m_diffuse.a == 0)
			continue;
		m_context->PSSetShader(dx11Viewer.m_mmdPS.Get(), nullptr, 0);
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
			ID3D11SamplerState* samplers[] = { dx11Viewer.m_textureSampler.Get() };
			m_context->PSSetShaderResources(0, 1, views);
			m_context->PSSetSamplers(0, 1, samplers);
		} else {
			psCB.m_textureModes.x = 0;
			ID3D11ShaderResourceView* views[] = { dx11Viewer.m_dummyTextureView.Get() };
			ID3D11SamplerState* samplers[] = { dx11Viewer.m_dummySampler.Get() };
			m_context->PSSetShaderResources(0, 1, views);
			m_context->PSSetSamplers(0, 1, samplers);
		}
		if (mat.m_toonTexture.m_texture) {
			psCB.m_textureModes.y = 1;
			psCB.m_toonTexMulFactor = mmdMat.m_toonTextureMulFactor;
			psCB.m_toonTexAddFactor = mmdMat.m_toonTextureAddFactor;
			ID3D11ShaderResourceView* views[] = { mat.m_toonTexture.m_textureView.Get() };
			ID3D11SamplerState* samplers[] = { dx11Viewer.m_toonTextureSampler.Get() };
			m_context->PSSetShaderResources(1, 1, views);
			m_context->PSSetSamplers(1, 1, samplers);
		} else {
			psCB.m_textureModes.y = 0;
			ID3D11ShaderResourceView* views[] = { dx11Viewer.m_dummyTextureView.Get() };
			ID3D11SamplerState* samplers[] = { dx11Viewer.m_dummySampler.Get() };
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
			ID3D11SamplerState* samplers[] = { dx11Viewer.m_sphereTextureSampler.Get() };
			m_context->PSSetShaderResources(2, 1, views);
			m_context->PSSetSamplers(2, 1, samplers);
		} else {
			psCB.m_textureModes.z = 0;
			ID3D11ShaderResourceView* views[] = { dx11Viewer.m_dummyTextureView.Get() };
			ID3D11SamplerState* samplers[] = { dx11Viewer.m_dummySampler.Get() };
			m_context->PSSetShaderResources(2, 1, views);
			m_context->PSSetSamplers(2, 1, samplers);
		}
		psCB.m_lightColor = viewer.m_lightColor;
		glm::vec3 lightDir = viewer.m_lightDir;
		auto viewMat = glm::mat3(viewer.m_viewMat);
		lightDir = viewMat * lightDir;
		psCB.m_lightDir = lightDir;
		m_context->UpdateSubresource(m_mmdPSConstantBuffer.Get(),
		                             0, nullptr, &psCB, 0, 0);
		ID3D11Buffer* pscbs[] = { m_mmdPSConstantBuffer.Get() };
		m_context->PSSetConstantBuffers(1, 1, pscbs);
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

	// Draw edge
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
		ID3D11Buffer* pscbs[] = { m_mmdEdgePSConstantBuffer.Get() };
		m_context->PSSetConstantBuffers(2, 1, pscbs);
		m_context->RSSetState(dx11Viewer.m_mmdEdgeRS.Get());
		m_context->OMSetBlendState(dx11Viewer.m_mmdEdgeBlendState.Get(), nullptr, 0xffffffff);
		m_context->DrawIndexed(m_vertexCount, m_beginIndex, 0);
	}

	// Draw ground shadow
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
	auto wsvp = dxMat * proj * view * shadow * world;
	DX11GroundShadowVertexShader vsCB{};
	vsCB.m_wvp = wsvp;
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
		ID3D11Buffer* pscbs[] = { m_mmdGroundShadowPSConstantBuffer.Get() };
		m_context->PSSetConstantBuffers(1, 1, pscbs);
		m_context->OMSetBlendState(dx11Viewer.m_mmdEdgeBlendState.Get(), nullptr, 0xffffffff);
		m_context->DrawIndexed(m_vertexCount, m_beginIndex, 0);
	}
}

bool DX11Viewer::Run(const SceneConfig& cfg) {
	MusicUtil music;
	music.Init(cfg.musicPath);
	if (!glfwInit())
		return false;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1920, 1080, "Pmx Mod", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		return false;
	}
	const HWND hwnd = glfwGetWin32Window(window);
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	D3D_FEATURE_LEVEL featureLevel;
	constexpr D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
		featureLevels, 1, D3D11_SDK_VERSION, &device, &featureLevel, &context))) {
		glfwTerminate();
		return false;
	}
	Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
	if (FAILED(device.As(&dxgiDevice))) {
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
	UINT msaaCount = 4;
	UINT quality = 0;
	if (FAILED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, msaaCount, &quality)) || quality == 0) {
		msaaCount = 1;
		quality = 0;
	}
	const UINT msaaQuality = quality > 0 ? quality - 1 : 0;
	m_multiSampleCount = msaaCount;
	m_multiSampleQuality = msaaQuality;
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	if (width <= 0 || height <= 0) {
		glfwTerminate();
		return false;
	}
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
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
	if (FAILED(factory->CreateSwapChain(device.Get(), &sd, &swapChain)))
		return false;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
	auto CreateRenderTargets = [&](const int w, const int h) -> bool {
		rtv.Reset();
		depthTex.Reset();
		dsv.Reset();
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
			return false;
		if (FAILED(device->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv)))
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
		if (FAILED(device->CreateTexture2D(&dsDesc, nullptr, &depthTex)))
			return false;
		if (FAILED(device->CreateDepthStencilView(depthTex.Get(), nullptr, &dsv)))
			return false;
		return true;
	};
	if (!CreateRenderTargets(width, height)) {
		glfwTerminate();
		return false;
	}
	if (!Setup(device.Get())) {
		glfwTerminate();
		return false;
	}
	LoadCameraVmd(cfg);
	std::vector<std::unique_ptr<Model>> models;
	if (!LoadModels(cfg, models)) {
		glfwTerminate();
		return false;
	}
	auto fpsTime  = std::chrono::steady_clock::now();
	auto saveTime = std::chrono::steady_clock::now();
	int fpsFrame  = 0;
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		int newW = 0, newH = 0;
		glfwGetFramebufferSize(window, &newW, &newH);
		if (newW != width || newH != height) {
			width = newW; height = newH;
			m_renderTargetView.Reset();
			m_depthStencilView.Reset();
			rtv.Reset(); dsv.Reset(); depthTex.Reset();
			if (FAILED(swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
				return false;
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
			if (SUCCEEDED(dx11Model.m_context->FinishCommandList(FALSE, &cmd)) && cmd)
				context->ExecuteCommandList(cmd.Get(), FALSE);
		}
		if (FAILED(swapChain->Present(0, 0)))
			return false;
		TickFps(fpsTime, fpsFrame);
	}
	models.clear();
	glfwDestroyWindow(window);
	glfwTerminate();
	return true;
}

std::unique_ptr<Model> DX11Viewer::CreateModel() const {
	return std::make_unique<DX11Model>();
}

bool DX11Viewer::Setup(const Microsoft::WRL::ComPtr<ID3D11Device>& device) {
	m_device = device;
	InitDirs("shader_DX11");
	if (!CreateShaders())
		return false;
	return true;
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
	auto CompileError = [](const Microsoft::WRL::ComPtr<ID3DBlob>& msg) {
		std::cout << "Compile Error: " << static_cast<char*>(msg->GetBufferPointer()) << std::endl;
		return false;
	};
	if (FAILED(D3DCompileFromFile((m_shaderDir / "mmd.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"VSMain", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
		&mmdVSBlob, &errorBlob)))
		return CompileError(errorBlob);
	if (FAILED(D3DCompileFromFile((m_shaderDir / "mmd.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PSMain", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
		&mmdPSBlob, &errorBlob)))
		return CompileError(errorBlob);
	if (FAILED(D3DCompileFromFile((m_shaderDir / "mmd_edge.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"VSMain", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
		&mmdEdgeVSBlob, &errorBlob)))
		return CompileError(errorBlob);
	if (FAILED(D3DCompileFromFile((m_shaderDir / "mmd_edge.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PSMain", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
		&mmdEdgePSBlob, &errorBlob)))
		return CompileError(errorBlob);
	if (FAILED(D3DCompileFromFile((m_shaderDir / "mmd_ground_shadow.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"VSMain", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
		&mmdGroundShadowVSBlob, &errorBlob)))
		return CompileError(errorBlob);
	if (FAILED(D3DCompileFromFile((m_shaderDir / "mmd_ground_shadow.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PSMain", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
		&mmdGroundShadowPSBlob, &errorBlob)))
		return CompileError(errorBlob);

	// mmd shader_GLFW
	if (FAILED(m_device->CreateVertexShader(
		mmdVSBlob->GetBufferPointer(), mmdVSBlob->GetBufferSize(),
		nullptr, &m_mmdVS)))
		return false;
	if (FAILED(m_device->CreatePixelShader(
		mmdPSBlob->GetBufferPointer(), mmdPSBlob->GetBufferSize(),
		nullptr, &m_mmdPS)))
		return false;
	D3D11_INPUT_ELEMENT_DESC mmdInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(m_device->CreateInputLayout(
		mmdInputElementDesc, 3,
		mmdVSBlob->GetBufferPointer(), mmdVSBlob->GetBufferSize(),
		&m_mmdInputLayout)))
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
	if (FAILED(m_device->CreateSamplerState(&texSamplerDesc, &m_textureSampler)))
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
	if (FAILED(m_device->CreateSamplerState(&toonSamplerDesc, &m_toonTextureSampler)))
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
	if (FAILED(m_device->CreateSamplerState(&spSamplerDesc, &m_sphereTextureSampler)))
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
	if (FAILED(m_device->CreateBlendState(&blendDesc1, &m_mmdBlendState)))
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
	if (FAILED(m_device->CreateRasterizerState(&frontRsDesc, &m_mmdFrontFaceRS)))
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
	if (FAILED(m_device->CreateRasterizerState(&faceRsDesc, &m_mmdBothFaceRS)))
		return false;

	// mmd edge shader_GLFW
	if (FAILED(m_device->CreateVertexShader(
		mmdEdgeVSBlob->GetBufferPointer(), mmdEdgeVSBlob->GetBufferSize(),
		nullptr, &m_mmdEdgeVS)))
		return false;
	if (FAILED(m_device->CreatePixelShader(
		mmdEdgePSBlob->GetBufferPointer(), mmdEdgePSBlob->GetBufferSize(),
		nullptr, &m_mmdEdgePS)))
		return false;
	D3D11_INPUT_ELEMENT_DESC mmdEdgeInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
			0, 0,
			D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,
			0, D3D11_APPEND_ALIGNED_ELEMENT,
			D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(m_device->CreateInputLayout(
		mmdEdgeInputElementDesc, 2,
		mmdEdgeVSBlob->GetBufferPointer(), mmdEdgeVSBlob->GetBufferSize(),
		&m_mmdEdgeInputLayout)))
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
	if (FAILED(m_device->CreateBlendState(&blendDesc2, &m_mmdEdgeBlendState)))
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
	if (FAILED(m_device->CreateRasterizerState(&rsDesc1, &m_mmdEdgeRS)))
		return false;

	// mmd ground shadow shader_GLFW
	if (FAILED(m_device->CreateVertexShader(
		mmdGroundShadowVSBlob->GetBufferPointer(), mmdGroundShadowVSBlob->GetBufferSize(),
		nullptr, &m_mmdGroundShadowVS)))
		return false;
	if (FAILED(m_device->CreatePixelShader(
		mmdGroundShadowPSBlob->GetBufferPointer(), mmdGroundShadowPSBlob->GetBufferSize(),
		nullptr, &m_mmdGroundShadowPS)))
		return false;

	D3D11_INPUT_ELEMENT_DESC mmdGroundShadowInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(m_device->CreateInputLayout(
		mmdGroundShadowInputElementDesc, 1,
		mmdGroundShadowVSBlob->GetBufferPointer(), mmdGroundShadowVSBlob->GetBufferSize(),
		&m_mmdGroundShadowInputLayout)))
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
	if (FAILED(m_device->CreateBlendState(&blendDesc3, &m_mmdGroundShadowBlendState)))
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
	if (FAILED(m_device->CreateRasterizerState(&rsDesc2, &m_mmdGroundShadowRS)))
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
	if (FAILED(m_device->CreateDepthStencilState(&stDesc, &m_mmdGroundShadowDSS)))
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
	if (FAILED(m_device->CreateDepthStencilState(&dstDesc, &m_defaultDSS)))
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
	if (FAILED(m_device->CreateTexture2D(&tex2dDesc, nullptr, &m_dummyTexture)))
		return false;
	if (FAILED(m_device->CreateShaderResourceView(m_dummyTexture.Get(), nullptr, &m_dummyTextureView)))
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
	if (FAILED(m_device->CreateSamplerState(&samplerDesc, &m_dummySampler)))
		return false;
	return true;
}
