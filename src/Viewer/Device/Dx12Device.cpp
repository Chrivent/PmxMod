#include "Viewer/Device/Dx12Device.h"

#include "Util.h"

#include <iterator>

namespace Chrivent {
	bool Dx12Device::SupportsMsaaSampleCount(ID3D12Device* device, const UINT sampleCount) {
		if (device == nullptr)
			return false;
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS colorQualityLevels{};
		colorQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		colorQualityLevels.SampleCount = sampleCount;
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS depthQualityLevels{};
		depthQualityLevels.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthQualityLevels.SampleCount = sampleCount;
		return SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
			&colorQualityLevels, sizeof(colorQualityLevels))) && colorQualityLevels.NumQualityLevels > 0
			&& SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
			&depthQualityLevels, sizeof(depthQualityLevels))) && depthQualityLevels.NumQualityLevels > 0;
	}

	UINT Dx12Device::ChooseMsaaSampleCount(ID3D12Device* device) {
		constexpr UINT sampleCounts[] = { 4u, 2u };
		for (const UINT sampleCount : sampleCounts) {
			if (SupportsMsaaSampleCount(device, sampleCount))
				return sampleCount;
		}
		return 1;
	}

	UINT Dx12Device::ResolveMaximumMsaaSampleCount(ID3D12Device* device) {
		constexpr UINT sampleCounts[] = { 32u, 16u, 8u, 4u, 2u };
		for (const UINT sampleCount : sampleCounts) {
			if (SupportsMsaaSampleCount(device, sampleCount))
				return sampleCount;
		}
		return 1;
	}

	const char* Dx12Device::ResolveFeatureLevelName(const D3D_FEATURE_LEVEL featureLevel) {
		switch (featureLevel) {
			case D3D_FEATURE_LEVEL_12_2: return "12.2";
			case D3D_FEATURE_LEVEL_12_1: return "12.1";
			case D3D_FEATURE_LEVEL_12_0: return "12.0";
			case D3D_FEATURE_LEVEL_11_1: return "11.1";
			default: return "11.0";
		}
	}

	const char* Dx12Device::ResolveShaderModelName(const D3D_SHADER_MODEL shaderModel) {
		switch (shaderModel) {
			case D3D_SHADER_MODEL_6_8: return "6.8";
			case D3D_SHADER_MODEL_6_7: return "6.7";
			case D3D_SHADER_MODEL_6_6: return "6.6";
			case D3D_SHADER_MODEL_6_5: return "6.5";
			case D3D_SHADER_MODEL_6_4: return "6.4";
			case D3D_SHADER_MODEL_6_3: return "6.3";
			case D3D_SHADER_MODEL_6_2: return "6.2";
			case D3D_SHADER_MODEL_6_1: return "6.1";
			case D3D_SHADER_MODEL_6_0: return "6.0";
			default: return "5.1";
		}
	}

	void Dx12Device::UpdateCapabilities(GraphicsCapabilities& capabilities,
		const DXGI_ADAPTER_DESC1& description) {
		constexpr D3D_FEATURE_LEVEL requestedFeatureLevels[] = {
			D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0
		};
		D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels;
		featureLevels.NumFeatureLevels = static_cast<UINT>(std::size(requestedFeatureLevels));
		featureLevels.pFeatureLevelsRequested = requestedFeatureLevels;
		featureLevels.MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels))))
			featureLevels.MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
		constexpr D3D_SHADER_MODEL requestedShaderModels[] = {
			D3D_SHADER_MODEL_6_8, D3D_SHADER_MODEL_6_7, D3D_SHADER_MODEL_6_6,
			D3D_SHADER_MODEL_6_5, D3D_SHADER_MODEL_6_4, D3D_SHADER_MODEL_6_3,
			D3D_SHADER_MODEL_6_2, D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0,
			D3D_SHADER_MODEL_5_1
		};
		maximumShaderModel = D3D_SHADER_MODEL_5_1;
		for (const D3D_SHADER_MODEL shaderModel : requestedShaderModels) {
			D3D12_FEATURE_DATA_SHADER_MODEL shaderModelData{ shaderModel };
			if (SUCCEEDED(device->CheckFeatureSupport(
				D3D12_FEATURE_SHADER_MODEL, &shaderModelData, sizeof(shaderModelData)))) {
				maximumShaderModel = shaderModelData.HighestShaderModel;
				break;
			}
		}
		D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12{};
		enhancedBarriersSupported = SUCCEEDED(device->CheckFeatureSupport(
			D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12))) && options12.EnhancedBarriersSupported;
		capabilities.apiName = "Direct3D 12";
		capabilities.apiVersion = std::string("Feature Level ") +
			ResolveFeatureLevelName(featureLevels.MaxSupportedFeatureLevel);
		capabilities.shaderVersion = std::string("Shader Model ") + ResolveShaderModelName(maximumShaderModel);
		capabilities.gpuName = Util::WStringToUtf8(description.Description);
		capabilities.gpuType = description.DedicatedVideoMemory > 0 ? "discrete" : "integrated";
		capabilities.maxSampleCount = ResolveMaximumMsaaSampleCount(device.Get());
		capabilities.activeSampleCount = msaaSampleCount;
		capabilities.uniformBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		capabilities.maxTextureBindings = D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
		capabilities.supportsEnhancedBarriers = enhancedBarriersSupported;
	}

	Dx12Device::~Dx12Device() {
		Shutdown();
	}

	GraphicsError::Result<void> Dx12Device::Initialize(GraphicsCapabilities& capabilities) {
		Shutdown();
		capabilities = {};
		HRESULT result = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InitializationFailed, "DXGI factory 생성",
				"DirectX 12 어댑터 검색용 factory를 만들지 못했습니다", result, true));
		}
		HRESULT lastDeviceResult = E_FAIL;
		for (UINT index = 0; ; index++) {
			Microsoft::WRL::ComPtr<IDXGIAdapter1> newAdapter;
			if (FAILED(factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&newAdapter))))
				break;
			DXGI_ADAPTER_DESC1 desc{};
			if (FAILED(newAdapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
				continue;
			lastDeviceResult = D3D12CreateDevice(newAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
			if (SUCCEEDED(lastDeviceResult)) {
				adapter = newAdapter;
				break;
			}
		}
		if (!device) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InitializationFailed, "device 초기화",
				"DirectX 12를 지원하는 고성능 그래픽 어댑터를 찾지 못했습니다", lastDeviceResult, true));
		}
		msaaSampleCount = ChooseMsaaSampleCount(device.Get());
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		result = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InitializationFailed, "command queue 생성",
				"DirectX 12 direct command queue를 만들지 못했습니다", result, true));
		}
		DXGI_ADAPTER_DESC1 description{};
		result = adapter->GetDesc1(&description);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InitializationFailed, "그래픽 어댑터 정보 조회",
				"DirectX 12 그래픽 어댑터 정보를 가져오지 못했습니다", result, true));
		}
		UpdateCapabilities(capabilities, description);
		return {};
	}

	void Dx12Device::Shutdown() {
		commandQueue.Reset();
		device.Reset();
		adapter.Reset();
		factory.Reset();
		msaaSampleCount = 1;
		maximumShaderModel = D3D_SHADER_MODEL_5_1;
		enhancedBarriersSupported = false;
	}
}
