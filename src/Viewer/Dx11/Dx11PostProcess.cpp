#include "Viewer/Dx11/Dx11PostProcess.h"

#include "Viewer/Dx11/Helper/Dx11DescBuilder.h"
#include "Viewer/Shader/PostProcessInputLayout.h"

namespace Chrivent {
	void Dx11PostProcess::ApplyViewport(ID3D11DeviceContext* context, const int width, const int height) {
		if (context == nullptr)
			return;
		D3D11_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(width);
		viewport.Height = static_cast<float>(height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		context->RSSetViewports(1, &viewport);
	}

	void Dx11PostProcess::InitializeFocusHistory(ID3D11DeviceContext* context) {
		if (focusHistoryInitialized || context == nullptr || !focusHistoryView[0] || !focusHistoryView[1])
			return;
		constexpr float clearHistory[4] = {};
		for (const auto& view : focusHistoryView)
			context->ClearRenderTargetView(view.Get(), clearHistory);
		focusHistoryIndex = 0;
		focusHistoryInitialized = true;
	}

	void Dx11PostProcess::UpdateFocusHistory(ID3D11DeviceContext* context, const int width, const int height) {
		if (!focusHistoryEnabled || context == nullptr)
			return;
		InitializeFocusHistory(context);
		const int readIndex = focusHistoryIndex;
		const int writeIndex = ResolveNextFocusHistoryIndex(focusHistoryIndex);
		ID3D11RenderTargetView* targetView = focusHistoryView[writeIndex].Get();
		context->OMSetRenderTargets(1, &targetView, nullptr);
		ApplyViewport(context, 1, 1);
		context->VSSetShader(focusHistoryShader.vertexShader.Get(), nullptr, 0);
		context->PSSetShader(focusHistoryShader.pixelShader.Get(), nullptr, 0);
		ID3D11ShaderResourceView* views[PostProcessInputLayout::RequiredTextureCount] = {
			sceneColorView.Get(), depthView.Get(), focusHistoryResourceView[readIndex].Get()
		};
		context->PSSetShaderResources(PostProcessInputLayout::SceneColorRegister,
			PostProcessInputLayout::RequiredTextureCount, views);
		context->Draw(3, 0);
		ID3D11ShaderResourceView* emptyViews[PostProcessInputLayout::RequiredTextureCount] = {};
		context->PSSetShaderResources(PostProcessInputLayout::SceneColorRegister,
			PostProcessInputLayout::RequiredTextureCount, emptyViews);
		focusHistoryIndex = writeIndex;
		ApplyViewport(context, width, height);
	}

	void Dx11PostProcess::ResetShaders() {
		ClearEffects();
		postProcessShaders.clear();
		focusHistoryShader = {};
		focusHistoryEnabled = false;
		ResetFocusHistory();
	}

	bool Dx11PostProcess::InitializeTargets(
		ID3D11Device* device, ID3D11DeviceContext* context, const int width, const int height) {
		ResetTargets();
		if (device == nullptr || context == nullptr || width <= 0 || height <= 0)
			return false;
		const auto sceneColorDesc = Dx11DescBuilder::MakeTexture2DDesc(
			width, height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		if (FAILED(device->CreateTexture2D(&sceneColorDesc, nullptr, &sceneColor))
			|| FAILED(device->CreateShaderResourceView(sceneColor.Get(), nullptr, &sceneColorView)))
			return false;
		const auto pingPongDesc = Dx11DescBuilder::MakeTexture2DDesc(width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		for (int index = 0; index < 2; index++) {
			if (FAILED(device->CreateTexture2D(&pingPongDesc, nullptr, &pingPongColor[index]))
				|| FAILED(device->CreateRenderTargetView(
					pingPongColor[index].Get(), nullptr, &pingPongColorView[index]))
				|| FAILED(device->CreateShaderResourceView(
					pingPongColor[index].Get(), nullptr, &pingPongColorResourceView[index])))
				return false;
		}
		const auto depthDesc = Dx11DescBuilder::MakeTexture2DDesc(width, height, DXGI_FORMAT_R24G8_TYPELESS,
			D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
		if (FAILED(device->CreateTexture2D(&depthDesc, nullptr, &depth)))
			return false;
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilDesc{};
		depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		if (FAILED(device->CreateDepthStencilView(depth.Get(), &depthStencilDesc, &depthStencilView)))
			return false;
		D3D11_SHADER_RESOURCE_VIEW_DESC depthResourceDesc{};
		depthResourceDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		depthResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		depthResourceDesc.Texture2D.MipLevels = 1;
		if (FAILED(device->CreateShaderResourceView(depth.Get(), &depthResourceDesc, &depthView)))
			return false;
		const auto focusHistoryDesc = Dx11DescBuilder::MakeTexture2DDesc(1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		for (int index = 0; index < 2; index++) {
			if (FAILED(device->CreateTexture2D(&focusHistoryDesc, nullptr, &focusHistory[index]))
				|| FAILED(device->CreateRenderTargetView(
					focusHistory[index].Get(), nullptr, &focusHistoryView[index]))
				|| FAILED(device->CreateShaderResourceView(
					focusHistory[index].Get(), nullptr, &focusHistoryResourceView[index])))
				return false;
		}
		InitializeFocusHistory(context);
		return focusHistoryInitialized;
	}

	void Dx11PostProcess::ResolveSceneColor(
		ID3D11DeviceContext* context, ID3D11Texture2D* source, const UINT sampleCount) const {
		if (context == nullptr || source == nullptr || !sceneColor)
			return;
		context->OMSetRenderTargets(0, nullptr, nullptr);
		if (sampleCount > 1)
			context->ResolveSubresource(sceneColor.Get(), 0, source, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			context->CopyResource(sceneColor.Get(), source);
	}

	bool Dx11PostProcess::Load(ID3D11Device* device, const std::vector<const EffectDefinition*>& effects) {
		ResetShaders();
		SetEffects(effects);
		for (const auto* effect : ResolveEffectPointers()) {
			const auto& pass = effect->passes.front();
			Dx11PostProcessShader shader;
			if (!shader.Initialize(device, pass.shaderPath, pass.vertexEntry.c_str(), pass.pixelEntry.c_str())) {
				ResetShaders();
				return false;
			}
			postProcessShaders.push_back(std::move(shader));
			if (IsDepthOfFieldEffect(*effect)) {
				const auto focusShaderPath = ResolveFocusUpdateShaderPath(pass);
				if (!focusHistoryShader.Initialize(device, focusShaderPath, "VSMain", "PSMain")) {
					ResetShaders();
					return false;
				}
				focusHistoryEnabled = true;
			}
		}
		ResetFocusHistory();
		return true;
	}

	void Dx11PostProcess::ClearShaders() {
		ResetShaders();
	}

	bool Dx11PostProcess::BeginDepthPass(ID3D11DeviceContext* context,
		ID3D11DepthStencilState* depthStencilState, const int width, const int height) const {
		if (!HasEffects() || context == nullptr || !depthStencilView)
			return false;
		ID3D11ShaderResourceView* emptyViews[PostProcessInputLayout::RequiredTextureCount] = {};
		context->PSSetShaderResources(PostProcessInputLayout::SceneColorRegister,
			PostProcessInputLayout::RequiredTextureCount, emptyViews);
		context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		context->OMSetRenderTargets(0, nullptr, depthStencilView.Get());
		context->OMSetDepthStencilState(depthStencilState, 0x00);
		context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
		ApplyViewport(context, width, height);
		return true;
	}

	void Dx11PostProcess::EndDepthPass(ID3D11DeviceContext* context) {
		if (context != nullptr)
			context->OMSetRenderTargets(0, nullptr, nullptr);
	}

	void Dx11PostProcess::Draw(ID3D11DeviceContext* context, ID3D11RenderTargetView* backBufferView,
		ID3D11RasterizerState* rasterizerState, ID3D11SamplerState* sampler, const int width, const int height) {
		if (!HasEffects() || context == nullptr || backBufferView == nullptr)
			return;
		InitializeFocusHistory(context);
		context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
		context->OMSetDepthStencilState(nullptr, 0);
		context->RSSetState(rasterizerState);
		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->PSSetSamplers(PostProcessInputLayout::LinearClampSamplerRegister, 1, &sampler);
		UpdateFocusHistory(context, width, height);
		ID3D11ShaderResourceView* sourceView = sceneColorView.Get();
		for (size_t index = 0; index < postProcessShaders.size(); index++) {
			const PostProcessPassRoute route = ResolvePingPongRoute(index, postProcessShaders.size());
			ID3D11RenderTargetView* targetView = route.lastPass
				? backBufferView : pingPongColorView[route.pingPongIndex].Get();
			context->OMSetRenderTargets(1, &targetView, nullptr);
			context->VSSetShader(postProcessShaders[index].vertexShader.Get(), nullptr, 0);
			context->PSSetShader(postProcessShaders[index].pixelShader.Get(), nullptr, 0);
			ID3D11ShaderResourceView* views[PostProcessInputLayout::RequiredTextureCount] = {
				sourceView, depthView.Get(), focusHistoryResourceView[focusHistoryIndex].Get()
			};
			context->PSSetShaderResources(PostProcessInputLayout::SceneColorRegister,
				PostProcessInputLayout::RequiredTextureCount, views);
			context->Draw(3, 0);
			ID3D11ShaderResourceView* emptyViews[PostProcessInputLayout::RequiredTextureCount] = {};
			context->PSSetShaderResources(PostProcessInputLayout::SceneColorRegister,
				PostProcessInputLayout::RequiredTextureCount, emptyViews);
			sourceView = pingPongColorResourceView[route.pingPongIndex].Get();
		}
	}

	void Dx11PostProcess::ResetFocusHistory() {
		focusHistoryIndex = 0;
		focusHistoryInitialized = false;
	}

	void Dx11PostProcess::ResetTargets() {
		sceneColor.Reset();
		sceneColorView.Reset();
		for (int index = 0; index < 2; index++) {
			pingPongColor[index].Reset();
			pingPongColorView[index].Reset();
			pingPongColorResourceView[index].Reset();
			focusHistory[index].Reset();
			focusHistoryView[index].Reset();
			focusHistoryResourceView[index].Reset();
		}
		depth.Reset();
		depthStencilView.Reset();
		depthView.Reset();
		ResetFocusHistory();
	}

	void Dx11PostProcess::Reset() {
		ResetShaders();
		ResetTargets();
	}
}
