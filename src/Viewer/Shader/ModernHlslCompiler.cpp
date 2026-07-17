#include "Viewer/Shader/ModernHlslCompiler.h"

#include "Viewer/Shader/SpirvBindingLayout.h"

#include <Windows.h>
#include <dxc/dxcapi.h>
#include <wrl/client.h>

namespace Chrivent {
	bool ModernHlslCompiler::CompileObject(const std::filesystem::path& file, const std::wstring& entry,
		const std::wstring& target, const std::span<const wchar_t* const> additionalArguments,
		std::vector<uint8_t>& outObject, std::string& outError) {
		Microsoft::WRL::ComPtr<IDxcUtils> utils;
		Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
		if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))) ||
			FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)))) {
			outError = "Failed to initialize the modern HLSL compiler.";
			return false;
		}
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
		if (FAILED(utils->CreateDefaultIncludeHandler(&includeHandler))) {
			outError = "Failed to initialize the HLSL include handler.";
			return false;
		}
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;
		if (FAILED(utils->LoadFile(file.c_str(), nullptr, &source))) {
			outError = "Failed to open HLSL shader: " + file.string();
			return false;
		}
		const DxcBuffer sourceBuffer{
			.Ptr = source->GetBufferPointer(),
			.Size = source->GetBufferSize(),
			.Encoding = DXC_CP_UTF8
		};
		const std::wstring includeDirectory = file.parent_path().wstring();
		std::vector arguments{
			file.c_str(), L"-E", entry.c_str(), L"-T", target.c_str(),
			L"-O3", L"-I", includeDirectory.c_str()
		};
		arguments.insert(arguments.end(), additionalArguments.begin(), additionalArguments.end());
		Microsoft::WRL::ComPtr<IDxcResult> result;
		if (FAILED(compiler->Compile(&sourceBuffer, arguments.data(), static_cast<uint32_t>(arguments.size()),
			includeHandler.Get(), IID_PPV_ARGS(&result)))) {
			outError = "Failed to invoke the modern HLSL compiler: " + file.string();
			return false;
		}
		Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
		if (FAILED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr)))
			errors.Reset();
		HRESULT status = E_FAIL;
		if (FAILED(result->GetStatus(&status))) {
			outError = "Failed to query HLSL compilation status: " + file.string();
			return false;
		}
		if (FAILED(status)) {
			outError = "Failed to compile HLSL shader: " + file.string();
			if (errors && errors->GetStringLength() > 0) {
				outError.push_back('\n');
				outError.append(errors->GetStringPointer(), errors->GetStringLength());
			}
			return false;
		}
		Microsoft::WRL::ComPtr<IDxcBlob> object;
		if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object), nullptr)) || !object) {
			outError = "The modern HLSL compiler returned an invalid shader object: " + file.string();
			return false;
		}
		outObject.resize(object->GetBufferSize());
		std::memcpy(outObject.data(), object->GetBufferPointer(), outObject.size());
		outError.clear();
		return true;
	}

	bool ModernHlslCompiler::CompileDxil(const std::filesystem::path& file, const std::wstring& entry,
		const std::wstring& target, std::vector<uint8_t>& outDxil, std::string& outError) {
		return CompileObject(file, entry, target, {}, outDxil, outError);
	}

	bool ModernHlslCompiler::CompileSpirv(const std::filesystem::path& file, const std::wstring& entry,
		const std::wstring& target, const SpirvTarget spirvTarget, const SpirvBindingProfile bindingProfile,
		std::vector<uint32_t>& outSpirv, std::string& outError, const bool invertVertexY) {
		const wchar_t* targetEnvironment = spirvTarget == SpirvTarget::OpenGl
			? L"-fspv-target-env=vulkan1.0"
			: L"-fspv-target-env=vulkan1.3";
		std::vector arguments{ L"-spirv", targetEnvironment };
		if (invertVertexY)
			arguments.emplace_back(L"-fvk-invert-y");
		std::vector<std::wstring> argumentStorage;
		const uint32_t textureCount = SpirvBindingLayout::ResolveTextureCount(bindingProfile);
		const uint32_t samplerCount = SpirvBindingLayout::ResolveSamplerCount(bindingProfile);
		argumentStorage.reserve((2 + textureCount + samplerCount) * 3);
		const auto AppendBinding = [&arguments, &argumentStorage](const wchar_t type, const uint32_t slot,
			const uint32_t binding, const uint32_t set) {
			argumentStorage.emplace_back(std::wstring(1, type) + std::to_wstring(slot));
			argumentStorage.emplace_back(std::to_wstring(binding));
			argumentStorage.emplace_back(std::to_wstring(set));
			const size_t index = argumentStorage.size() - 3;
			arguments.emplace_back(L"-fvk-bind-register");
			arguments.emplace_back(argumentStorage[index].c_str());
			arguments.emplace_back(L"0");
			arguments.emplace_back(argumentStorage[index + 1].c_str());
			arguments.emplace_back(argumentStorage[index + 2].c_str());
		};
		const bool openGl = spirvTarget == SpirvTarget::OpenGl;
		AppendBinding(L'b', SpirvBindingLayout::ResolveFrameDataRegister(bindingProfile),
			SpirvBindingLayout::frameDataBinding, openGl ? 0 : SpirvBindingLayout::frameDataSet);
		const uint32_t parameterDataRegister = SpirvBindingLayout::ResolveParameterDataRegister(bindingProfile);
		AppendBinding(L'b', parameterDataRegister,
			openGl ? parameterDataRegister : SpirvBindingLayout::parameterDataBinding,
			openGl ? 0 : SpirvBindingLayout::parameterDataSet);
		for (uint32_t slot = 0; slot < textureCount; slot++)
			AppendBinding(L't', slot, SpirvBindingLayout::ResolveTextureBinding(slot),
				openGl ? 0 : SpirvBindingLayout::textureSet);
		for (uint32_t slot = 0; slot < samplerCount; slot++)
			AppendBinding(L's', slot, SpirvBindingLayout::ResolveSamplerBinding(slot),
				openGl ? 0 : SpirvBindingLayout::textureSet);
		std::vector<uint8_t> object;
		if (!CompileObject(file, entry, target, arguments, object, outError))
			return false;
		if (object.size() % sizeof(uint32_t) != 0) {
			outError = "The HLSL compiler returned invalid SPIR-V: " + file.string();
			return false;
		}
		outSpirv.resize(object.size() / sizeof(uint32_t));
		std::memcpy(outSpirv.data(), object.data(), object.size());
		return true;
	}
}
