#include "Viewer/Shader/DxcHlslCompiler.h"

#include "Viewer/Shader/SpirvBindingLayout.h"

#include <Windows.h>
#include <dxc/dxcapi.h>
#include <utility>
#include <wrl/client.h>

namespace Chrivent {
	ShaderCompileError::Result<std::vector<uint8_t>> DxcHlslCompiler::CompileObject(
		const std::filesystem::path& file, const std::wstring& entry,
		const std::wstring& target, const std::span<const wchar_t* const> additionalArguments) {
		Microsoft::WRL::ComPtr<IDxcUtils> utils;
		Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
		if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))) ||
			FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)))) {
			return std::unexpected(ShaderCompileError{
				.shaderPath = file,
				.message = "DXC를 초기화하지 못했습니다."
			});
		}
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
		if (FAILED(utils->CreateDefaultIncludeHandler(&includeHandler))) {
			return std::unexpected(ShaderCompileError{
				.shaderPath = file,
				.message = "HLSL include handler를 초기화하지 못했습니다."
			});
		}
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;
		if (FAILED(utils->LoadFile(file.c_str(), nullptr, &source))) {
			return std::unexpected(ShaderCompileError{
				.shaderPath = file,
				.message = "HLSL 셰이더를 열지 못했습니다: " + file.string()
			});
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
			return std::unexpected(ShaderCompileError{
				.shaderPath = file,
				.message = "DXC를 실행하지 못했습니다: " + file.string()
			});
		}
		Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
		if (FAILED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr)))
			errors.Reset();
		HRESULT status = E_FAIL;
		if (FAILED(result->GetStatus(&status))) {
			return std::unexpected(ShaderCompileError{
				.shaderPath = file,
				.message = "HLSL 컴파일 상태를 확인하지 못했습니다: " + file.string()
			});
		}
		if (FAILED(status)) {
			std::string message = "HLSL 셰이더를 컴파일하지 못했습니다: " + file.string();
			if (errors && errors->GetStringLength() > 0) {
				message.push_back('\n');
				message.append(errors->GetStringPointer(), errors->GetStringLength());
			}
			return std::unexpected(ShaderCompileError{
				.shaderPath = file,
				.message = std::move(message)
			});
		}
		Microsoft::WRL::ComPtr<IDxcBlob> object;
		if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object), nullptr)) || !object) {
			return std::unexpected(ShaderCompileError{
				.shaderPath = file,
				.message = "DXC가 올바르지 않은 셰이더 객체를 반환했습니다: " + file.string()
			});
		}
		std::vector<uint8_t> compiledObject(object->GetBufferSize());
		std::memcpy(compiledObject.data(), object->GetBufferPointer(), compiledObject.size());
		return compiledObject;
	}

	ShaderCompileError::Result<std::vector<uint8_t>> DxcHlslCompiler::CompileDxil(
		const std::filesystem::path& file, const std::wstring& entry, const std::wstring& target) {
		return CompileObject(file, entry, target, {});
	}

	ShaderCompileError::Result<std::vector<uint32_t>> DxcHlslCompiler::CompileSpirv(
		const std::filesystem::path& file, const std::wstring& entry,
		const std::wstring& target, const SpirvTarget spirvTarget, const SpirvBindingProfile bindingProfile,
		const bool invertVertexY) {
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
		auto objectResult = CompileObject(file, entry, target, arguments);
		if (!objectResult)
			return std::unexpected(std::move(objectResult.error()));
		const std::vector<uint8_t>& object = *objectResult;
		if (object.size() % sizeof(uint32_t) != 0) {
			return std::unexpected(ShaderCompileError{
				.shaderPath = file,
				.message = "DXC가 올바르지 않은 SPIR-V를 반환했습니다: " + file.string()
			});
		}
		std::vector<uint32_t> spirv(object.size() / sizeof(uint32_t));
		std::memcpy(spirv.data(), object.data(), object.size());
		return spirv;
	}
}
