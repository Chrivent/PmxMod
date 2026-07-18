#include "Viewer/PostProcess/Dx11PostProcess.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Viewer/DrawContext/Dx11DrawContext.h"
#include "Viewer/PostProcess/PostProcessFrameData.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"

#include <algorithm>
#include <utility>

namespace Chrivent {
	bool Dx11PostProcess::CreateEffectResources(ID3D11Device* device) {
		ResetEffectResources();
		if (device == nullptr)
			return false;
		const auto& plans = GetResourcePlans();
		resources.resize(plans.size());
		for (size_t resourceIndex = 0; resourceIndex < plans.size(); resourceIndex++) {
			const PostProcessResourcePlan& plan = plans[resourceIndex];
			auto& [textures
				, renderTargetViews
				, shaderResourceViews] = resources[resourceIndex];
			const size_t textureCount = plan.lifetime == EffectResourceLifetime::History ? 2 : 1;
			const DXGI_FORMAT format = plan.format == EffectTextureFormat::Rgba8Unorm
				? DXGI_FORMAT_R8G8B8A8_UNORM
				: plan.format == EffectTextureFormat::Rgba16Float
					? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT;
			const auto description = Dx11DescBuilder::MakeTexture2DDesc(
				ResolveResourceExtent(targetWidth, plan, true), ResolveResourceExtent(targetHeight, plan, false),
				format, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
			for (size_t index = 0; index < textureCount; index++) {
				if (FAILED(device->CreateTexture2D(&description, nullptr, &textures[index]))
					|| FAILED(device->CreateRenderTargetView(
						textures[index].Get(), nullptr, &renderTargetViews[index]))
					|| FAILED(device->CreateShaderResourceView(
						textures[index].Get(), nullptr, &shaderResourceViews[index])))
					return false;
			}
		}
		ResetHistory();
		return true;
	}

	void Dx11PostProcess::InitializeHistories(ID3D11DeviceContext* context) {
		if (context == nullptr)
			return;
		const auto& plans = GetResourcePlans();
		constexpr float clearColor[4]{};
		for (size_t index = 0; index < resources.size() && index < plans.size(); index++) {
			const Resource& resource = resources[index];
			if (!NeedsHistoryInitialization(index))
				continue;
			context->ClearRenderTargetView(resource.renderTargetViews[0].Get(), clearColor);
			context->ClearRenderTargetView(resource.renderTargetViews[1].Get(), clearColor);
			MarkHistoryInitialized(index);
		}
	}

	ID3D11ShaderResourceView* Dx11PostProcess::ResolveInputView(
		const PostProcessPassInputRoute& input) const {
		if (input.kind == PostProcessInputKind::SceneColor)
			return sceneColorView.Get();
		if (input.kind == PostProcessInputKind::SceneDepth)
			return depthView.Get();
		if (input.kind == PostProcessInputKind::SceneVelocity)
			return velocityView.Get();
		if (input.resourceIndex >= resources.size())
			return sceneColorView.Get();
		const Resource& resource = resources[input.resourceIndex];
		return resource.shaderResourceViews[ResolveResourceReadIndex(input.resourceIndex)].Get();
	}

	ID3D11RenderTargetView* Dx11PostProcess::ResolveOutputView(
		const PostProcessPassRoute& route, ID3D11RenderTargetView* backBufferView) const {
		if (route.outputKind == PostProcessOutputKind::Present)
			return backBufferView;
		if (route.outputResourceIndex >= resources.size())
			return nullptr;
		const Resource& resource = resources[route.outputResourceIndex];
		return resource.renderTargetViews[ResolveResourceWriteIndex(route.outputResourceIndex)].Get();
	}

	void Dx11PostProcess::ResetEffectResources() {
		resources.clear();
	}

	void Dx11PostProcess::ResetShaders() {
		postProcessShaders.clear();
		ResetHistory();
	}

	bool Dx11PostProcess::InitializeTargets(
		ID3D11Device* device, ID3D11DeviceContext* context, const int width, const int height) {
		ResetTargets();
		if (device == nullptr || context == nullptr || width <= 0 || height <= 0)
			return false;
		targetWidth = width;
		targetHeight = height;
		D3D11_BUFFER_DESC frameDataDesc{};
		frameDataDesc.ByteWidth = static_cast<UINT>(sizeof(PostProcessFrameData));
		frameDataDesc.Usage = D3D11_USAGE_DEFAULT;
		frameDataDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(device->CreateBuffer(&frameDataDesc, nullptr, &frameDataBuffer)))
			return false;
		frameDataDesc.ByteWidth = static_cast<UINT>(sizeof(PostProcessParameterData));
		if (FAILED(device->CreateBuffer(&frameDataDesc, nullptr, &parameterDataBuffer)))
			return false;
		const auto sceneColorDesc = Dx11DescBuilder::MakeTexture2DDesc(
			width, height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		if (FAILED(device->CreateTexture2D(&sceneColorDesc, nullptr, &sceneColor))
			|| FAILED(device->CreateShaderResourceView(sceneColor.Get(), nullptr, &sceneColorView)))
			return false;
		if (RequiresDepth() || RequiresVelocity()) {
			const auto depthDesc = Dx11DescBuilder::MakeTexture2DDesc(
				width, height, DXGI_FORMAT_R24G8_TYPELESS,
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
		}
		if (RequiresVelocity()) {
			const auto velocityDesc = Dx11DescBuilder::MakeTexture2DDesc(
				width, height, DXGI_FORMAT_R16G16_FLOAT,
				D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
			if (FAILED(device->CreateTexture2D(&velocityDesc, nullptr, &velocity))
				|| FAILED(device->CreateRenderTargetView(
					velocity.Get(), nullptr, &velocityRenderTargetView))
				|| FAILED(device->CreateShaderResourceView(velocity.Get(), nullptr, &velocityView)))
				return false;
		}
		return CreateEffectResources(device);
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

	bool Dx11PostProcess::CreateShaders(ID3D11Device* device, std::string& error) {
		ResetShaders();
		error.clear();
		for (const auto& [shaderPath, vertexEntry, pixelEntry] : GetShaderPrograms()) {
			Dx11PostProcessShader shader;
			if (!shader.Initialize(device, shaderPath,
				vertexEntry.c_str(), pixelEntry.c_str(), error))
				return false;
			postProcessShaders.push_back(std::move(shader));
		}
		return true;
	}

	void Dx11PostProcess::SwapResources(Dx11PostProcess& other) noexcept {
		sceneColor.Swap(other.sceneColor);
		sceneColorView.Swap(other.sceneColorView);
		depth.Swap(other.depth);
		depthStencilView.Swap(other.depthStencilView);
		depthView.Swap(other.depthView);
		velocity.Swap(other.velocity);
		velocityRenderTargetView.Swap(other.velocityRenderTargetView);
		velocityView.Swap(other.velocityView);
		frameDataBuffer.Swap(other.frameDataBuffer);
		parameterDataBuffer.Swap(other.parameterDataBuffer);
		resources.swap(other.resources);
		postProcessShaders.swap(other.postProcessShaders);
		std::swap(targetWidth, other.targetWidth);
		std::swap(targetHeight, other.targetHeight);
	}

	GraphicsResult<void> Dx11PostProcess::Configure(ID3D11Device* device,
		ID3D11DeviceContext* context,
		const int width, const int height, const std::vector<const EffectRuntimeDefinition*>& effects) {
		Dx11PostProcess candidate;
		const auto planResult = candidate.SetEffects(effects);
		if (!planResult) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ContractViolation, "후처리 실행 계획 생성", planResult.error()));
		}
		if (candidate.HasEffects()
			&& !candidate.InitializeTargets(device, context, width, height)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 target 생성",
				"DirectX 11 후처리 texture와 view를 만들지 못했습니다"));
		}
		std::string error;
		if (candidate.HasEffects() && !candidate.CreateShaders(device, error)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::EffectConfigurationFailed, "후처리 셰이더 생성",
				error.empty() ? "DirectX 11 후처리 셰이더를 만들지 못했습니다" : std::move(error)));
		}
		SwapExecutionPlan(candidate);
		SwapResources(candidate);
		return {};
	}

	bool Dx11PostProcess::BeginSceneInputPass(ID3D11DeviceContext* context,
		ID3D11DepthStencilState* depthStencilState, const int width, const int height) const {
		if ((!RequiresDepth() && !RequiresVelocity()) || context == nullptr || !depthStencilView)
			return false;
		ID3D11ShaderResourceView* emptyViews[PostProcessInputLayout::maxTextureCount]{};
		context->PSSetShaderResources(0, std::size(emptyViews), emptyViews);
		context->ClearDepthStencilView(
			depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		ID3D11RenderTargetView* velocityTarget = RequiresVelocity() ? velocityRenderTargetView.Get() : nullptr;
		if (velocityTarget != nullptr) {
			constexpr float velocityClear[4]{};
			context->ClearRenderTargetView(velocityTarget, velocityClear);
		}
		context->OMSetRenderTargets(velocityTarget != nullptr ? 1 : 0,
			velocityTarget != nullptr ? &velocityTarget : nullptr, depthStencilView.Get());
		context->OMSetDepthStencilState(depthStencilState, 0x00);
		context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
		Dx11DrawContext::ApplyViewport(context, width, height);
		return true;
	}

	void Dx11PostProcess::EndSceneInputPass(ID3D11DeviceContext* context) {
		if (context != nullptr)
			context->OMSetRenderTargets(0, nullptr, nullptr);
	}

	bool Dx11PostProcess::Draw(ID3D11DeviceContext* context, ID3D11RenderTargetView* backBufferView,
		ID3D11RasterizerState* rasterizerState, ID3D11SamplerState* sampler,
		const int width, const int height, const PostProcessFrameData& frameData) {
		if (!HasEffects() || context == nullptr || backBufferView == nullptr
			|| !IsPassCountCompatible(postProcessShaders.size()))
			return false;
		BeginHistoryFrame();
		context->UpdateSubresource(frameDataBuffer.Get(), 0, nullptr, &frameData, 0, 0);
		context->PSSetConstantBuffers(0, 1, frameDataBuffer.GetAddressOf());
		context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
		context->OMSetDepthStencilState(nullptr, 0);
		context->RSSetState(rasterizerState);
		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ID3D11SamplerState* samplers[PostProcessInputLayout::samplerCount]{};
		for (auto& currentSampler : samplers)
			currentSampler = sampler;
		context->PSSetSamplers(PostProcessInputLayout::linearClampSamplerRegister,
			PostProcessInputLayout::samplerCount, samplers);
		InitializeHistories(context);
		const auto& routes = GetPassRoutes();
		for (size_t index = 0; index < routes.size(); index++) {
			const PostProcessPassRoute& route = routes[index];
			const PostProcessParameterData& parameterData = GetParameterData(route);
			context->UpdateSubresource(parameterDataBuffer.Get(), 0, nullptr, &parameterData, 0, 0);
			context->PSSetConstantBuffers(PostProcessInputLayout::parameterDataRegister,
				1, parameterDataBuffer.GetAddressOf());
			ID3D11RenderTargetView* targetView = ResolveOutputView(route, backBufferView);
			context->OMSetRenderTargets(1, &targetView, nullptr);
			int outputWidth = width;
			int outputHeight = height;
			ResolveOutputExtent(route, outputWidth, outputHeight);
			Dx11DrawContext::ApplyViewport(context, outputWidth, outputHeight);
			context->VSSetShader(postProcessShaders[index].vertexShader.Get(), nullptr, 0);
			context->PSSetShader(postProcessShaders[index].pixelShader.Get(), nullptr, 0);
			ID3D11ShaderResourceView* views[PostProcessInputLayout::maxTextureCount]{};
			for (auto& view : views)
				view = sceneColorView.Get();
			for (const auto& input : route.inputs)
				views[input.slot] = ResolveInputView(input);
			context->PSSetShaderResources(0, PostProcessInputLayout::maxTextureCount, views);
			context->Draw(3, 0);
			for (auto& view : views)
				view = nullptr;
			context->PSSetShaderResources(0, PostProcessInputLayout::maxTextureCount, views);
			AdvanceHistory(route);
		}
		return true;
	}

	void Dx11PostProcess::ResetTargets() {
		ResetEffectResources();
		sceneColor.Reset();
		sceneColorView.Reset();
		depth.Reset();
		depthStencilView.Reset();
		depthView.Reset();
		velocity.Reset();
		velocityRenderTargetView.Reset();
		velocityView.Reset();
		frameDataBuffer.Reset();
		parameterDataBuffer.Reset();
		targetWidth = 0;
		targetHeight = 0;
	}

	void Dx11PostProcess::ResetResources() {
		ResetShaders();
		ResetTargets();
	}
}
