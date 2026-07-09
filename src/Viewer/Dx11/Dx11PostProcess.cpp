#include "Viewer/Dx11/Dx11PostProcess.h"

#include "Viewer/Dx11/Dx11Viewer.h"
#include "Viewer/Shader/PostProcessInputLayout.h"

#include <filesystem>

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

	void Dx11PostProcess::UpdateFocusHistory(
		const Dx11DeviceResources& resources, const Dx11RenderTargets& targets, const int width, const int height) {
		if (!focusHistoryEnabled)
			return;
		ID3D11DeviceContext* context = resources.context.Get();
		const int readIndex = focusHistoryIndex;
		const int writeIndex = 1 - focusHistoryIndex;
		ID3D11RenderTargetView* targetView = targets.focusHistoryView[writeIndex].Get();
		context->OMSetRenderTargets(1, &targetView, nullptr);
		ApplyViewport(context, 1, 1);
		context->VSSetShader(focusHistoryShader.vertexShader.Get(), nullptr, 0);
		context->PSSetShader(focusHistoryShader.pixelShader.Get(), nullptr, 0);
		ID3D11ShaderResourceView* views[PostProcessInputLayout::RequiredTextureCount] = {
			targets.sceneColorView.Get(),
			targets.postProcessDepthView.Get(),
			targets.focusHistoryResourceView[readIndex].Get()
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

	bool Dx11PostProcess::Load(ID3D11Device* device, const std::vector<const EffectDefinition*>& effects) {
		Reset();
		SetEffects(effects);
		for (const auto* effect : ResolveEffectPointers()) {
			const auto& pass = effect->passes.front();
			Dx11PostProcessShader shader;
			if (!shader.Initialize(device, pass.shaderPath, pass.vertexEntry.c_str(), pass.pixelEntry.c_str())) {
				Reset();
				return false;
			}
			postProcessShaders.push_back(std::move(shader));
			if (effect->id == "depth-of-field") {
				const auto focusShaderPath = pass.shaderPath.parent_path() / "focus-update.hlsl";
				if (std::filesystem::exists(focusShaderPath)
					&& focusHistoryShader.Initialize(device, focusShaderPath, "VSMain", "PSMain"))
					focusHistoryEnabled = true;
			}
		}
		focusHistoryIndex = 0;
		return true;
	}

	bool Dx11PostProcess::BeginDepthPass(
		const Dx11DeviceResources& resources, const Dx11RenderTargets& targets,
		const Dx11PipelineStates& pipelineStates, const int width, const int height) const {
		if (!HasEffects())
			return false;
		ID3D11DeviceContext* context = resources.context.Get();
		ID3D11ShaderResourceView* emptyViews[PostProcessInputLayout::RequiredTextureCount] = {};
		context->PSSetShaderResources(PostProcessInputLayout::SceneColorRegister,
			PostProcessInputLayout::RequiredTextureCount, emptyViews);
		context->ClearDepthStencilView(
			targets.postProcessDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		context->OMSetRenderTargets(0, nullptr, targets.postProcessDepthStencilView.Get());
		context->OMSetDepthStencilState(pipelineStates.defaultDss.Get(), 0x00);
		context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
		ApplyViewport(context, width, height);
		return true;
	}

	void Dx11PostProcess::EndDepthPass(const Dx11DeviceResources& resources) {
		resources.context->OMSetRenderTargets(0, nullptr, nullptr);
	}

	void Dx11PostProcess::Draw(
		const Dx11DeviceResources& resources, const Dx11RenderTargets& targets,
		const Dx11PipelineStates& pipelineStates, const int width, const int height) {
		ID3D11DeviceContext* context = resources.context.Get();
		context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
		context->OMSetDepthStencilState(nullptr, 0);
		context->RSSetState(pipelineStates.bothFaceRs.Get());
		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->PSSetSamplers(PostProcessInputLayout::LinearClampSamplerRegister, 1, pipelineStates.toonTextureSampler.GetAddressOf());
		UpdateFocusHistory(resources, targets, width, height);
		ID3D11ShaderResourceView* sourceView = targets.sceneColorView.Get();
		for (size_t index = 0; index < postProcessShaders.size(); index++) {
			const bool lastPass = index + 1 == postProcessShaders.size();
			const size_t targetIndex = index % 2;
			ID3D11RenderTargetView* targetView = lastPass
				? targets.backBufferView.Get()
				: targets.pingPongColorView[targetIndex].Get();
			context->OMSetRenderTargets(1, &targetView, nullptr);
			context->VSSetShader(postProcessShaders[index].vertexShader.Get(), nullptr, 0);
			context->PSSetShader(postProcessShaders[index].pixelShader.Get(), nullptr, 0);
			ID3D11ShaderResourceView* views[PostProcessInputLayout::RequiredTextureCount] = {
				sourceView,
				targets.postProcessDepthView.Get(),
				focusHistoryEnabled
					? targets.focusHistoryResourceView[focusHistoryIndex].Get()
					: targets.postProcessDepthView.Get()
			};
			context->PSSetShaderResources(PostProcessInputLayout::SceneColorRegister,
				PostProcessInputLayout::RequiredTextureCount, views);
			context->Draw(3, 0);
			ID3D11ShaderResourceView* emptyViews[PostProcessInputLayout::RequiredTextureCount] = {};
			context->PSSetShaderResources(PostProcessInputLayout::SceneColorRegister,
				PostProcessInputLayout::RequiredTextureCount, emptyViews);
			sourceView = targets.pingPongColorResourceView[targetIndex].Get();
		}
		ID3D11ShaderResourceView* emptyViews[PostProcessInputLayout::RequiredTextureCount] = {};
		context->PSSetShaderResources(PostProcessInputLayout::SceneColorRegister,
			PostProcessInputLayout::RequiredTextureCount, emptyViews);
	}

	void Dx11PostProcess::Reset() {
		ClearEffects();
		postProcessShaders.clear();
		focusHistoryShader = {};
		focusHistoryEnabled = false;
		focusHistoryIndex = 0;
	}
}
