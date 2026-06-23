#include "Viewer/Shader/DxcShaderCompiler.h"

#include <Windows.h>
#include <dxc/dxcapi.h>
#include <iterator>
#include <wrl/client.h>

namespace Chrivent {
	bool DxcShaderCompiler::CompileSpirv(const std::filesystem::path& file, const std::wstring& entry,
		const std::wstring& target, const SpirvTarget spirvTarget,
		std::vector<uint32_t>& outSpirv, std::string& outError, const bool invertVertexY) {
		Microsoft::WRL::ComPtr<IDxcUtils> utils;
		Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
		if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)))
			|| FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)))) {
			outError = "Failed to initialize DXC.";
			return false;
		}
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
		if (FAILED(utils->CreateDefaultIncludeHandler(&includeHandler))) {
			outError = "Failed to initialize the DXC include handler.";
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
		const wchar_t* targetEnvironment = spirvTarget == SpirvTarget::OpenGl
			? L"-fspv-target-env=vulkan1.0"
			: L"-fspv-target-env=vulkan1.2";
		const std::wstring includeDirectory = file.parent_path().wstring();
		std::vector arguments{
			file.c_str(), L"-E", entry.c_str(), L"-T", target.c_str(), L"-spirv",
			targetEnvironment, L"-O3", L"-I", includeDirectory.c_str()
		};
		if (invertVertexY)
			arguments.emplace_back(L"-fvk-invert-y");
		if (spirvTarget == SpirvTarget::OpenGl) {
			const wchar_t* openGlBindings[] = {
				L"-fvk-bind-register", L"b0", L"0", L"0", L"0",
				L"-fvk-bind-register", L"b1", L"0", L"1", L"0",
				L"-fvk-bind-register", L"t0", L"0", L"0", L"0",
				L"-fvk-bind-register", L"t1", L"0", L"1", L"0",
				L"-fvk-bind-register", L"t2", L"0", L"2", L"0",
				L"-fvk-bind-register", L"s0", L"0", L"3", L"0",
				L"-fvk-bind-register", L"s1", L"0", L"4", L"0",
				L"-fvk-bind-register", L"s2", L"0", L"5", L"0"
			};
			arguments.insert(arguments.end(), std::begin(openGlBindings), std::end(openGlBindings));
		} else {
			const wchar_t* vulkanBindings[] = {
				L"-fvk-bind-register", L"b0", L"0", L"0", L"0",
				L"-fvk-bind-register", L"b1", L"0", L"0", L"1",
				L"-fvk-bind-register", L"t0", L"0", L"0", L"2",
				L"-fvk-bind-register", L"t1", L"0", L"1", L"2",
				L"-fvk-bind-register", L"t2", L"0", L"2", L"2",
				L"-fvk-bind-register", L"s0", L"0", L"3", L"2",
				L"-fvk-bind-register", L"s1", L"0", L"4", L"2",
				L"-fvk-bind-register", L"s2", L"0", L"5", L"2"
			};
			arguments.insert(arguments.end(), std::begin(vulkanBindings), std::end(vulkanBindings));
		}
		Microsoft::WRL::ComPtr<IDxcResult> result;
		if (FAILED(compiler->Compile(&sourceBuffer, arguments.data(), static_cast<uint32_t>(arguments.size()),
			includeHandler.Get(), IID_PPV_ARGS(&result)))) {
			outError = "Failed to invoke DXC: " + file.string();
			return false;
		}
		Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
		const HRESULT errorsResult = result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
		HRESULT status = E_FAIL;
		const HRESULT statusResult = result->GetStatus(&status);
		if (FAILED(statusResult)) {
			outError = "Failed to query DXC compilation status: " + file.string();
			return false;
		}
		if (FAILED(status)) {
			outError = "Failed to compile HLSL shader: " + file.string();
			if (SUCCEEDED(errorsResult) && errors && errors->GetStringLength() > 0) {
				outError.push_back('\n');
				outError.append(errors->GetStringPointer(), errors->GetStringLength());
			}
			return false;
		}
		Microsoft::WRL::ComPtr<IDxcBlob> object;
		if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object), nullptr))
			|| object->GetBufferSize() % sizeof(uint32_t) != 0) {
			outError = "DXC returned invalid SPIR-V: " + file.string();
			return false;
		}
		const auto* words = static_cast<const uint32_t*>(object->GetBufferPointer());
		outSpirv.assign(words, words + object->GetBufferSize() / sizeof(uint32_t));
		outError.clear();
		return true;
	}
}
