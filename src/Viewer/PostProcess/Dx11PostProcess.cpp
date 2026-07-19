#include "Viewer/PostProcess/Dx11PostProcess.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Viewer/DrawContext/Dx11DrawContext.h"
#include "Viewer/PostProcess/PostProcessFrameData.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"

#include <algorithm>
#include <utility>

namespace Chrivent {
	GraphicsError::Result<void> Dx11PostProcess::WriteConstantBuffer(ID3D11DeviceContext* context,
		ID3D11Buffer* buffer, const void* data, const size_t size, const char* operation) {
		if (context == nullptr || buffer == nullptr || data == nullptr || size == 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidArgument, operation,
				"DirectX 11 후처리 상수 버퍼 입력이 올바르지 않습니다"));
		}
		D3D11_MAPPED_SUBRESOURCE mappedResource{};
		const HRESULT result = context->Map(
			buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::CommandRecordingFailed, operation,
				"DirectX 11 후처리 상수 버퍼를 매핑하지 못했습니다", result, true));
		}
		std::memcpy(mappedResource.pData, data, size);
		context->Unmap(buffer, 0);
		return {};
	}

	GraphicsError::Result<void> Dx11PostProcess::CreateEffectResources(ID3D11Device* device) {
		ResetEffectResources();
		if (device == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidArgument, "후처리 effect target 생성",
				"DirectX 11 device를 사용할 수 없습니다"));
		}
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
				HRESULT result = device->CreateTexture2D(&description, nullptr, &textures[index]);
				if (FAILED(result)) {
					return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
						GraphicsErrorCode::ResourceCreationFailed, "후처리 effect texture 생성",
						"DirectX 11 후처리 effect texture를 만들지 못했습니다", result, true));
				}
				result = device->CreateRenderTargetView(
					textures[index].Get(), nullptr, &renderTargetViews[index]);
				if (FAILED(result)) {
					return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
						GraphicsErrorCode::ResourceCreationFailed, "후처리 effect render target view 생성",
						"DirectX 11 후처리 effect render target view를 만들지 못했습니다", result, true));
				}
				result = device->CreateShaderResourceView(
					textures[index].Get(), nullptr, &shaderResourceViews[index]);
				if (FAILED(result)) {
					return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
						GraphicsErrorCode::ResourceCreationFailed, "후처리 effect shader resource view 생성",
						"DirectX 11 후처리 effect shader resource view를 만들지 못했습니다", result, true));
				}
			}
		}
		ResetHistory();
		return {};
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
			return nullptr;
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

	void Dx11PostProcess::ResetPrograms() {
		postProcessPrograms.clear();
		ResetHistory();
	}

	GraphicsError::Result<void> Dx11PostProcess::InitializeTargets(
		ID3D11Device* device, const int width, const int height) {
		ResetTargets();
		if (device == nullptr || width <= 0 || height <= 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidArgument, "후처리 target 생성",
				"DirectX 11 device 또는 후처리 target 크기가 올바르지 않습니다"));
		}
		targetWidth = width;
		targetHeight = height;
		D3D11_BUFFER_DESC frameDataDesc{};
		frameDataDesc.ByteWidth = static_cast<UINT>(sizeof(PostProcessFrameData));
		frameDataDesc.Usage = D3D11_USAGE_DYNAMIC;
		frameDataDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		frameDataDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT result = device->CreateBuffer(&frameDataDesc, nullptr, &frameDataBuffer);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 frame buffer 생성",
				"DirectX 11 후처리 frame constant buffer를 만들지 못했습니다", result, true));
		}
		frameDataDesc.ByteWidth = static_cast<UINT>(sizeof(PostProcessParameterData));
		result = device->CreateBuffer(&frameDataDesc, nullptr, &parameterDataBuffer);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 parameter buffer 생성",
				"DirectX 11 후처리 parameter constant buffer를 만들지 못했습니다", result, true));
		}
		const auto sceneColorDesc = Dx11DescBuilder::MakeTexture2DDesc(
			width, height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
		result = device->CreateTexture2D(&sceneColorDesc, nullptr, &sceneColor);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 scene color 생성",
				"DirectX 11 후처리 scene color texture를 만들지 못했습니다", result, true));
		}
		result = device->CreateShaderResourceView(sceneColor.Get(), nullptr, &sceneColorView);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 scene color view 생성",
				"DirectX 11 후처리 scene color view를 만들지 못했습니다", result, true));
		}
		if (RequiresDepth() || RequiresVelocity()) {
			const auto depthDesc = Dx11DescBuilder::MakeTexture2DDesc(
				width, height, DXGI_FORMAT_R24G8_TYPELESS,
				D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
			result = device->CreateTexture2D(&depthDesc, nullptr, &depth);
			if (FAILED(result)) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 depth texture 생성",
					"DirectX 11 후처리 depth texture를 만들지 못했습니다", result, true));
			}
			D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilDesc{};
			depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depthStencilDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			result = device->CreateDepthStencilView(depth.Get(), &depthStencilDesc, &depthStencilView);
			if (FAILED(result)) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 depth stencil view 생성",
					"DirectX 11 후처리 depth stencil view를 만들지 못했습니다", result, true));
			}
			D3D11_SHADER_RESOURCE_VIEW_DESC depthResourceDesc{};
			depthResourceDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			depthResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			depthResourceDesc.Texture2D.MipLevels = 1;
			result = device->CreateShaderResourceView(depth.Get(), &depthResourceDesc, &depthView);
			if (FAILED(result)) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 depth view 생성",
					"DirectX 11 후처리 depth shader resource view를 만들지 못했습니다", result, true));
			}
		}
		if (RequiresVelocity()) {
			const auto velocityDesc = Dx11DescBuilder::MakeTexture2DDesc(
				width, height, DXGI_FORMAT_R16G16_FLOAT,
				D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
			result = device->CreateTexture2D(&velocityDesc, nullptr, &velocity);
			if (FAILED(result)) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 velocity texture 생성",
					"DirectX 11 후처리 velocity texture를 만들지 못했습니다", result, true));
			}
			result = device->CreateRenderTargetView(
				velocity.Get(), nullptr, &velocityRenderTargetView);
			if (FAILED(result)) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 velocity render target view 생성",
					"DirectX 11 후처리 velocity render target view를 만들지 못했습니다", result, true));
			}
			result = device->CreateShaderResourceView(velocity.Get(), nullptr, &velocityView);
			if (FAILED(result)) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 velocity shader resource view 생성",
					"DirectX 11 후처리 velocity shader resource view를 만들지 못했습니다", result, true));
			}
		}
		return CreateEffectResources(device);
	}

	GraphicsError::Result<void> Dx11PostProcess::CreatePrograms(ID3D11Device* device) {
		ResetPrograms();
		for (const auto& program : GetShaderPrograms()) {
			Dx11ShaderProgram postProcessProgram;
			const auto result = postProcessProgram.Initialize(device, program);
			if (!result)
				return std::unexpected(result.error());
			postProcessPrograms.push_back(std::move(postProcessProgram));
		}
		return {};
	}

	GraphicsError::Result<void> Dx11PostProcess::CreateStates(ID3D11Device* device) {
		auto rasterizerDescription = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		HRESULT result = device->CreateRasterizerState(
			&rasterizerDescription, &fullscreenRasterizerState);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 rasterizer state 생성",
				"DirectX 11 후처리 rasterizer state를 만들지 못했습니다", result, true));
		}
		const auto samplerDescription = Dx11DescBuilder::MakeSamplerDesc(
			D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
		result = device->CreateSamplerState(&samplerDescription, &linearClampSampler);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 sampler state 생성",
				"DirectX 11 후처리 linear clamp sampler를 만들지 못했습니다", result, true));
		}
		return {};
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
		fullscreenRasterizerState.Swap(other.fullscreenRasterizerState);
		linearClampSampler.Swap(other.linearClampSampler);
		resources.swap(other.resources);
		postProcessPrograms.swap(other.postProcessPrograms);
		std::swap(targetWidth, other.targetWidth);
		std::swap(targetHeight, other.targetHeight);
	}

	GraphicsError::Result<void> Dx11PostProcess::Configure(ID3D11Device* device,
		const int width, const int height, const std::vector<const EffectRuntimeDefinition*>& effects) {
		Dx11PostProcess candidate;
		const auto planResult = candidate.SetEffects(effects);
		if (!planResult) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::ContractViolation, "후처리 실행 계획 생성", planResult.error().Format()));
		}
		if (candidate.HasEffects()) {
			const auto targetResult = candidate.InitializeTargets(device, width, height);
			if (!targetResult)
				return std::unexpected(targetResult.error());
			const auto programResult = candidate.CreatePrograms(device);
			if (!programResult)
				return std::unexpected(programResult.error());
			const auto stateResult = candidate.CreateStates(device);
			if (!stateResult)
				return std::unexpected(stateResult.error());
		}
		SwapExecutionPlan(candidate);
		SwapResources(candidate);
		return {};
	}

	GraphicsError::Result<void> Dx11PostProcess::BeginSceneInputPass(ID3D11DeviceContext* context,
		ID3D11DepthStencilState* depthStencilState, const int width, const int height) const {
		if ((!RequiresDepth() && !RequiresVelocity()) || context == nullptr || !depthStencilView) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 시작",
				"DirectX 11 context 또는 후처리 장면 입력 target이 준비되지 않았습니다"));
		}
		ID3D11ShaderResourceView* emptyViews[PostProcessInputLayout::maxTextureCount]{};
		context->PSSetShaderResources(
			PostProcessInputLayout::firstTextureRegister, std::size(emptyViews), emptyViews);
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
		return {};
	}

	GraphicsError::Result<void> Dx11PostProcess::EndSceneInputPass(ID3D11DeviceContext* context) {
		if (context == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 종료",
				"DirectX 11 context를 사용할 수 없습니다"));
		}
		context->OMSetRenderTargets(0, nullptr, nullptr);
		return {};
	}

	GraphicsError::Result<void> Dx11PostProcess::Draw(
		ID3D11DeviceContext* context, ID3D11Texture2D* sceneSource, const UINT sampleCount,
		ID3D11RenderTargetView* backBufferView, const int width, const int height,
		const PostProcessFrameData& frameData) {
		if (!HasEffects() || context == nullptr || backBufferView == nullptr
			|| sceneSource == nullptr || !sceneColor || !sceneColorView
			|| !frameDataBuffer || !parameterDataBuffer || !fullscreenRasterizerState
			|| !linearClampSampler
			|| !IsPassCountCompatible(postProcessPrograms.size())) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidState, "후처리 효과 draw",
				"DirectX 11 후처리 리소스 또는 실행 계획이 준비되지 않았습니다"));
		}
		context->OMSetRenderTargets(0, nullptr, nullptr);
		if (sampleCount > 1)
			context->ResolveSubresource(sceneColor.Get(), 0, sceneSource, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			context->CopyResource(sceneColor.Get(), sceneSource);
		const auto frameWriteResult = WriteConstantBuffer(context, frameDataBuffer.Get(),
			&frameData, sizeof(frameData), "후처리 frame 상수 기록");
		if (!frameWriteResult)
			return std::unexpected(frameWriteResult.error());
		BeginHistoryFrame();
		context->PSSetConstantBuffers(
			PostProcessInputLayout::frameDataRegister, 1, frameDataBuffer.GetAddressOf());
		context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
		context->OMSetDepthStencilState(nullptr, 0);
		context->RSSetState(fullscreenRasterizerState.Get());
		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ID3D11SamplerState* samplers[PostProcessInputLayout::samplerCount]{};
		for (auto& currentSampler : samplers)
			currentSampler = linearClampSampler.Get();
		context->PSSetSamplers(PostProcessInputLayout::linearClampSamplerRegister,
			PostProcessInputLayout::samplerCount, samplers);
		context->PSSetConstantBuffers(PostProcessInputLayout::parameterDataRegister,
			1, parameterDataBuffer.GetAddressOf());
		InitializeHistories(context);
		const auto& routes = GetPassRoutes();
		size_t parameterEffectIndex = routes.size();
		for (size_t index = 0; index < routes.size(); index++) {
			const PostProcessPassRoute& route = routes[index];
			if (parameterEffectIndex != route.effectIndex) {
				const PostProcessParameterData& parameterData = GetParameterData(route);
				const auto parameterWriteResult = WriteConstantBuffer(context,
					parameterDataBuffer.Get(), &parameterData, sizeof(parameterData),
					"후처리 parameter 상수 기록");
				if (!parameterWriteResult) {
					DiscardHistoryFrame();
					return std::unexpected(parameterWriteResult.error());
				}
				parameterEffectIndex = route.effectIndex;
			}
			ID3D11RenderTargetView* targetView = ResolveOutputView(route, backBufferView);
			if (targetView == nullptr) {
				DiscardHistoryFrame();
				return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
					GraphicsErrorCode::ContractViolation, "후처리 출력 target 조회",
					"DirectX 11 후처리 pass의 출력 target을 찾지 못했습니다"));
			}
			context->OMSetRenderTargets(1, &targetView, nullptr);
			int outputWidth = width;
			int outputHeight = height;
			ResolveOutputExtent(route, outputWidth, outputHeight);
			Dx11DrawContext::ApplyViewport(context, outputWidth, outputHeight);
			context->VSSetShader(postProcessPrograms[index].GetVertexShader(), nullptr, 0);
			context->PSSetShader(postProcessPrograms[index].GetPixelShader(), nullptr, 0);
			ID3D11ShaderResourceView* views[PostProcessInputLayout::maxTextureCount]{};
			for (auto& view : views)
				view = sceneColorView.Get();
			for (const auto& input : route.inputs) {
				views[input.slot] = ResolveInputView(input);
				if (views[input.slot] == nullptr) {
					DiscardHistoryFrame();
					return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
						GraphicsErrorCode::ContractViolation, "후처리 입력 texture 조회",
						"DirectX 11 후처리 pass의 입력 texture를 찾지 못했습니다"));
				}
			}
			context->PSSetShaderResources(PostProcessInputLayout::firstTextureRegister,
				PostProcessInputLayout::maxTextureCount, views);
			context->Draw(3, 0);
			for (auto& view : views)
				view = nullptr;
			context->PSSetShaderResources(PostProcessInputLayout::firstTextureRegister,
				PostProcessInputLayout::maxTextureCount, views);
			AdvanceHistory(route);
		}
		return {};
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
		ResetPrograms();
		ResetTargets();
		fullscreenRasterizerState.Reset();
		linearClampSampler.Reset();
	}
}
