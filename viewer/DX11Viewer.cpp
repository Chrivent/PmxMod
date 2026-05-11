#include "DX11Viewer.h"

#include "../src/Model.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <d3dcompiler.h>
#include <stb_image.h>

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

template<typename T>
HRESULT CreateBuffer(ID3D11Device* device, Microsoft::WRL::ComPtr<ID3D11Buffer>& out) {
	const UINT bytes = static_cast<UINT>(sizeof(T) + 15u & ~15u);
	const CD3D11_BUFFER_DESC desc(bytes, D3D11_BIND_CONSTANT_BUFFER);
	return device->CreateBuffer(&desc, nullptr, out.GetAddressOf());
}

void BindTexture(ID3D11DeviceContext* context, ID3D11ShaderResourceView* dummySRV, ID3D11SamplerState* dummySampler,
	const UINT slot, const DX11Texture& tex, ID3D11SamplerState* sampler, const int modeIfPresent, int& outMode,
	glm::vec4& outMul, glm::vec4& outAdd, const glm::vec4& mulIn, const glm::vec4& addIn) {
	if (tex.texture) {
		outMode = modeIfPresent;
		outMul  = mulIn;
		outAdd  = addIn;
	} else
		outMode = 0;
	ID3D11ShaderResourceView* views = tex.texture ? tex.textureView.Get() : dummySRV;
	ID3D11SamplerState* samplers = tex.texture ? sampler : dummySampler;
	context->PSSetShaderResources(slot, 1, &views);
	context->PSSetSamplers(slot, 1, &samplers);
}

DX11Material::DX11Material(const Material& sourceMat)
	: mat(sourceMat) {
}

bool DX11Instance::Setup(Viewer& baseViewer) {
	viewer = &dynamic_cast<DX11Viewer&>(baseViewer);
	D3D11_BUFFER_DESC vBufDesc = {};
	vBufDesc.Usage = D3D11_USAGE_DYNAMIC;
	vBufDesc.ByteWidth = static_cast<UINT>(sizeof(DX11Vertex) * model->positions.size());
	vBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(viewer->device->CreateBuffer(&vBufDesc, nullptr, &vertexBuffer)))
		return false;
	D3D11_BUFFER_DESC iBufDesc = {};
	iBufDesc.Usage = D3D11_USAGE_IMMUTABLE;
	iBufDesc.ByteWidth = static_cast<UINT>(model->indexElementSize * model->indexCount);
	iBufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	iBufDesc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = model->indices.data();
	if (FAILED(viewer->device->CreateBuffer(&iBufDesc, &initData, &indexBuffer)))
		return false;
	if (1 == model->indexElementSize)
		indexBufferFormat = DXGI_FORMAT_R8_UINT;
	else if (2 == model->indexElementSize)
		indexBufferFormat = DXGI_FORMAT_R16_UINT;
	else if (4 == model->indexElementSize)
		indexBufferFormat = DXGI_FORMAT_R32_UINT;
	else
		return false;
	if (FAILED(CreateBuffer<DX11VertexShader>(viewer->device.Get(), vsConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11PixelShader>(viewer->device.Get(), psConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11EdgeVertexShader>(viewer->device.Get(), edgeVsConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11EdgeSizeVertexShader>(viewer->device.Get(), edgeSizeVsConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11EdgePixelShader>(viewer->device.Get(), edgePsConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11GroundShadowVertexShader>(viewer->device.Get(), gsVsConstantBuffer)))
		return false;
	if (FAILED(CreateBuffer<DX11GroundShadowPixelShader>(viewer->device.Get(), gsPsConstantBuffer)))
		return false;
	for (const auto& mat : model->materials) {
		DX11Material material(mat);
		if (!mat.texture.empty())
			material.texture = viewer->LoadTexture(mat.texture);
		if (!mat.spTexture.empty())
			material.spTexture = viewer->LoadTexture(mat.spTexture);
		if (!mat.cartoonTexture.empty())
			material.toonTexture = viewer->LoadTexture(mat.cartoonTexture);
		materials.emplace_back(std::move(material));
	}
	return true;
}

void DX11Instance::Update() const {
	model->Update();
	const size_t vtxCount = model->positions.size();
	D3D11_MAPPED_SUBRESOURCE mapRes;
	if (FAILED(viewer->context->Map(vertexBuffer.Get(), 0,
		D3D11_MAP_WRITE_DISCARD, 0, &mapRes)))
		return;
	const auto vertices = static_cast<DX11Vertex*>(mapRes.pData);
	const glm::vec3* updatePositionData = model->updatePositions.data();
	const glm::vec3* updateNormalData = model->updateNormals.data();
	const glm::vec2* updateUVData = model->updateUVs.data();
	for (size_t i = 0; i < vtxCount; i++) {
		vertices[i].position = updatePositionData[i];
		vertices[i].normal = updateNormalData[i];
		vertices[i].uv = updateUVData[i];
	}
	viewer->context->Unmap(vertexBuffer.Get(), 0);
}

void DX11Instance::Draw() const {
	const auto& view = viewer->viewMat;
	const auto& proj = viewer->projMat;
	const auto& dxMat = glm::mat4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.5f, 1.0f
	);
	auto world = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
	auto wv = view * world;
	auto wvp = dxMat * proj * view * world;
	viewer->context->OMSetDepthStencilState(viewer->defaultDss.Get(), 0x00);
	UINT stride = sizeof(DX11Vertex);
	UINT offset = 0;
	viewer->context->IASetInputLayout(viewer->inputLayout.Get());
	viewer->context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
	viewer->context->IASetIndexBuffer(indexBuffer.Get(), indexBufferFormat, 0);
	viewer->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DX11VertexShader vsCB1{};
	vsCB1.wv = wv;
	vsCB1.wvp = wvp;
	viewer->context->UpdateSubresource(vsConstantBuffer.Get(),
		0, nullptr, &vsCB1, 0, 0);
	viewer->context->VSSetShader(viewer->vs.Get(), nullptr, 0);
	viewer->context->PSSetShader(viewer->ps.Get(), nullptr, 0);
	viewer->context->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
	for (const auto& [beginIndex, indexCount, materialId] : model->subMeshes) {
        const auto& material = materials[materialId];
		const auto& mat = material.mat;
        if (mat.diffuse.a == 0)
            continue;
        DX11PixelShader psCB{};
        psCB.alpha         = mat.diffuse.a;
        psCB.diffuse       = mat.diffuse;
        psCB.ambient       = mat.ambient;
        psCB.specular      = mat.specular;
        psCB.specularPower = mat.specularPower;
		int baseMode = 0;
		if (material.texture.texture)
			baseMode = !material.texture.hasAlpha ? 1 : 2;
		BindTexture(viewer->context.Get(), viewer->dummyTextureView.Get(), viewer->textureSampler.Get(),
			0, material.texture, viewer->textureSampler.Get(), baseMode, psCB.textureModes.x,
			psCB.texMulFactor, psCB.texAddFactor, mat.textureMulFactor, mat.textureAddFactor);
		BindTexture(viewer->context.Get(), viewer->dummyTextureView.Get(), viewer->textureSampler.Get(),
			1, material.toonTexture, viewer->toonTextureSampler.Get(), 1, psCB.textureModes.y,
			psCB.toonTexMulFactor, psCB.toonTexAddFactor, mat.cartoonTextureMulFactor, mat.cartoonTextureAddFactor);
		int spMode = 0;
		if (material.spTexture.texture) {
			if (mat.spTextureMode == SphereMode::Mul)
				spMode = 1;
			else if (mat.spTextureMode == SphereMode::Add)
				spMode = 2;
		}
		BindTexture(viewer->context.Get(), viewer->dummyTextureView.Get(), viewer->textureSampler.Get(),
			2, material.spTexture, viewer->textureSampler.Get(), spMode, psCB.textureModes.z,
			psCB.sphereTexMulFactor, psCB.sphereTexAddFactor, mat.sphereTextureMulFactor, mat.sphereTextureAddFactor);
        psCB.lightColor = viewer->lightColor;
        psCB.lightDir = glm::mat3(viewer->viewMat) * viewer->lightDir;
        viewer->context->UpdateSubresource(psConstantBuffer.Get(), 0, nullptr, &psCB, 0, 0);
        viewer->context->PSSetConstantBuffers(1, 1, psConstantBuffer.GetAddressOf());
        if (mat.bothFace)
            viewer->context->RSSetState(viewer->bothFaceRs.Get());
        else
            viewer->context->RSSetState(viewer->frontFaceRs.Get());
        viewer->context->DrawIndexed(indexCount, beginIndex, 0);
    }
	viewer->context->IASetInputLayout(viewer->edgeInputLayout.Get());
	DX11EdgeVertexShader vsCB2{};
	vsCB2.wv = wv;
	vsCB2.wvp = wvp;
	vsCB2.screenSize = glm::vec2(static_cast<float>(viewer->screenWidth),
		static_cast<float>(viewer->screenHeight));
	viewer->context->UpdateSubresource(edgeVsConstantBuffer.Get(),
		0, nullptr, &vsCB2, 0, 0);
	viewer->context->VSSetShader(viewer->edgeVs.Get(), nullptr, 0);
	viewer->context->PSSetShader(viewer->edgePs.Get(), nullptr, 0);
	viewer->context->VSSetConstantBuffers(0, 1, edgeVsConstantBuffer.GetAddressOf());
	for (const auto& [beginIndex, indexCount, materialId] : model->subMeshes) {
		const auto& material = materials[materialId];
		const auto& mat = material.mat;
		if (!mat.edgeFlag)
			continue;
		if (mat.diffuse.a == 0)
			continue;
		DX11EdgeSizeVertexShader vsCB{};
		vsCB.edgeSize = mat.edgeSize;
		viewer->context->UpdateSubresource(edgeSizeVsConstantBuffer.Get(),
			0, nullptr, &vsCB, 0, 0);
		viewer->context->VSSetConstantBuffers(1, 1, edgeSizeVsConstantBuffer.GetAddressOf());
		DX11EdgePixelShader psCB{};
		psCB.edgeColor = mat.edgeColor;
		viewer->context->UpdateSubresource(edgePsConstantBuffer.Get(),
			0, nullptr, &psCB, 0, 0);
		viewer->context->PSSetConstantBuffers(2, 1, edgePsConstantBuffer.GetAddressOf());
		viewer->context->RSSetState(viewer->edgeRs.Get());
		viewer->context->DrawIndexed(indexCount, beginIndex, 0);
	}
	viewer->context->IASetInputLayout(viewer->gsInputLayout.Get());
	glm::vec4 plane(0.f, 1.f, 0.f, 0.f);
	glm::vec4 light(-glm::normalize(viewer->lightDir), 0.f);
	glm::mat4 shadow = glm::dot(plane, light) * glm::mat4(1.f) - glm::outerProduct(light, plane);
	DX11GroundShadowVertexShader vsCB3{};
	vsCB3.wvp = dxMat * proj * view * shadow * world;
	viewer->context->UpdateSubresource(gsVsConstantBuffer.Get(),
		0, nullptr, &vsCB3, 0, 0);
	viewer->context->VSSetShader(viewer->gsVs.Get(), nullptr, 0);
	viewer->context->PSSetShader(viewer->gsPs.Get(), nullptr, 0);
	viewer->context->VSSetConstantBuffers(0, 1, gsVsConstantBuffer.GetAddressOf());
	viewer->context->RSSetState(viewer->gsRs.Get());
	viewer->context->OMSetDepthStencilState(viewer->gsDss.Get(), 0x01);
	for (const auto& [beginIndex, indexCount, materialId] : model->subMeshes) {
		const auto& material = materials[materialId];
		const auto& mat = material.mat;
		if (!mat.groundShadow)
			continue;
		if (mat.diffuse.a == 0)
			continue;
		DX11GroundShadowPixelShader psCB{};
		psCB.shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
		viewer->context->UpdateSubresource(gsPsConstantBuffer.Get(), 0, nullptr, &psCB, 0, 0);
		viewer->context->PSSetConstantBuffers(1, 1, gsPsConstantBuffer.GetAddressOf());
		viewer->context->DrawIndexed(indexCount, beginIndex, 0);
	}
}

void DX11Viewer::ConfigureGlfwHints() {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

bool DX11Viewer::Setup() {
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
	DXGI_SWAP_CHAIN_DESC d{};
	d.BufferCount = 2;
	d.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	d.OutputWindow = hwnd;
	d.SampleDesc.Count = multiSampleCount;
	d.SampleDesc.Quality = multiSampleQuality;
	d.Windowed = TRUE;
	if (FAILED(factory->CreateSwapChain(device.Get(), &d, &swapChain)))
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

void DX11Viewer::BeginFrame() {
	context->ClearRenderTargetView(renderTargetView.Get(), clearColor);
	context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());
	context->OMSetBlendState(blendState.Get(), nullptr, 0xffffffff);
}

bool DX11Viewer::EndFrame() {
	if (FAILED(swapChain->Present(0, 0)))
		return false;
	return true;
}

std::unique_ptr<Instance> DX11Viewer::CreateInstance() const {
	return std::make_unique<DX11Instance>();
}

DX11Texture DX11Viewer::LoadTexture(const std::filesystem::path& texturePath) {
	const auto it = textures.find(texturePath);
	if (it != textures.end())
		return it->second;
	int x = 0, y = 0, comp = 0;
	stbi_uc* image = LoadImageRGBA(texturePath, x, y, comp);
	if (!image)
		return {};
	D3D11_TEXTURE2D_DESC d;
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
	const bool textureHasAlpha = comp == 4;
	d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = image;
	initData.SysMemPitch = 4 * x;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2d;
	const HRESULT hr = device->CreateTexture2D(&d, &initData, &tex2d);
	stbi_image_free(image);
	if (FAILED(hr))
		return {};
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tex2dRV;
	if (FAILED(device->CreateShaderResourceView(tex2d.Get(), nullptr, &tex2dRV)))
		return {};
	DX11Texture tex;
	tex.texture = tex2d;
	tex.textureView = tex2dRV;
	tex.hasAlpha = textureHasAlpha;
	textures[texturePath] = tex;
	return textures[texturePath];
}

void DX11Viewer::UpdateViewport() const {
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<float>(screenWidth);
	vp.Height = static_cast<float>(screenHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	context->RSSetViewports(1, &vp);
}

bool DX11Viewer::MakeVS(const std::filesystem::path& f, const char* entry,
	Microsoft::WRL::ComPtr<ID3D11VertexShader>& outVS, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob) const {
	if (FAILED(D3DCompileFromFile(f.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entry, "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &outBlob, nullptr)))
		return false;
	if (FAILED(device->CreateVertexShader(outBlob->GetBufferPointer(), outBlob->GetBufferSize(), nullptr, &outVS)))
		return false;
	return true;
}

bool DX11Viewer::MakePS(const std::filesystem::path& f, const char* entry, Microsoft::WRL::ComPtr<ID3D11PixelShader>& outPS) const {
	Microsoft::WRL::ComPtr<ID3DBlob> blob;
	if (FAILED(D3DCompileFromFile(f.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entry, "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, nullptr)))
		return false;
	if (FAILED(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &outPS)))
		return false;
	return true;
}

bool DX11Viewer::CreateShaders() {
	Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, edgeVsBlob, gsVsBlob;
	if (!MakeVS(shaderDir / "mmd.hlsl", "VSMain", vs, vsBlob))
		return false;
	if (!MakeVS(shaderDir / "mmd_edge.hlsl", "VSMain", edgeVs, edgeVsBlob))
		return false;
	if (!MakeVS(shaderDir / "mmd_ground_shadow.hlsl", "VSMain", gsVs, gsVsBlob))
		return false;
	if (!MakePS(shaderDir / "mmd.hlsl", "PSMain", ps))
		return false;
	if (!MakePS(shaderDir / "mmd_edge.hlsl", "PSMain", edgePs))
		return false;
	if (!MakePS(shaderDir / "mmd_ground_shadow.hlsl", "PSMain", gsPs))
		return false;
	constexpr D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(device->CreateInputLayout(
		inputElementDesc, 3,
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
		&inputLayout)))
		return false;
	constexpr D3D11_INPUT_ELEMENT_DESC edgeInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(device->CreateInputLayout(
		edgeInputElementDesc, 2,
		edgeVsBlob->GetBufferPointer(), edgeVsBlob->GetBufferSize(),
		&edgeInputLayout)))
		return false;
	constexpr D3D11_INPUT_ELEMENT_DESC gsInputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(device->CreateInputLayout(
		gsInputElementDesc, 1,
		gsVsBlob->GetBufferPointer(), gsVsBlob->GetBufferSize(),
		&gsInputLayout)))
		return false;
	return true;
}

bool DX11Viewer::CreateRenderTargets() {
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
		return false;
	if (FAILED(device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView)))
		return false;
	D3D11_TEXTURE2D_DESC d{};
	d.Width = static_cast<UINT>(screenWidth);
	d.Height = static_cast<UINT>(screenHeight);
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d.SampleDesc.Count = multiSampleCount;
	d.SampleDesc.Quality = multiSampleQuality;
	d.Usage = D3D11_USAGE_DEFAULT;
	d.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	if (FAILED(device->CreateTexture2D(&d, nullptr, &depthTex)))
		return false;
	if (FAILED(device->CreateDepthStencilView(depthTex.Get(), nullptr, &depthStencilView)))
		return false;
	return true;
}

bool DX11Viewer::CreatePipelineStates() {
	auto wrapLinear = Sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
	if (FAILED(device->CreateSamplerState(&wrapLinear, &textureSampler)))
		return false;
	auto clampLinear = Sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
	if (FAILED(device->CreateSamplerState(&clampLinear, &toonTextureSampler)))
		return false;
	auto blend = AlphaBlend();
	if (FAILED(device->CreateBlendState(&blend, &blendState)))
		return false;
	auto frontRsDesc = Raster(D3D11_CULL_BACK, true);
	if (FAILED(device->CreateRasterizerState(&frontRsDesc, &frontFaceRs)))
		return false;
	auto bothRsDesc = Raster(D3D11_CULL_NONE, true);
	if (FAILED(device->CreateRasterizerState(&bothRsDesc, &bothFaceRs)))
		return false;
	auto edgeRsDesc = Raster(D3D11_CULL_FRONT, true);
	edgeRsDesc.DepthClipEnable = FALSE;
	if (FAILED(device->CreateRasterizerState(&edgeRsDesc, &edgeRs)))
		return false;
	auto gsRsDesc = Raster(D3D11_CULL_NONE, true);
	gsRsDesc.DepthClipEnable = FALSE;
	gsRsDesc.DepthBias = -1;
	gsRsDesc.SlopeScaledDepthBias = -1.0f;
	gsRsDesc.DepthBiasClamp = -1.0f;
	if (FAILED(device->CreateRasterizerState(&gsRsDesc, &gsRs)))
		return false;
	auto gsDSS = MakeGroundShadowDepthStencilDesc();
	if (FAILED(device->CreateDepthStencilState(&gsDSS, &gsDss)))
		return false;
	auto defDSS = MakeDefaultDepthStencilDesc();
	if (FAILED(device->CreateDepthStencilState(&defDSS, &defaultDss)))
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
	if (FAILED(device->CreateTexture2D(&d, nullptr, &dummyTexture)))
		return false;
	if (FAILED(device->CreateShaderResourceView(dummyTexture.Get(), nullptr, &dummyTextureView)))
		return false;
	return true;
}

