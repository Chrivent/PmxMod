#include "Viewer/Shader/Dx11Shader.h"

#include "Viewer/Shader/D3DCompilerHlslCompiler.h"

#include <utility>

namespace Chrivent {
	GraphicsResult<void> Dx11ShaderProgram::CreateVertexShader(ID3D11Device* device,
		ID3DBlob* bytecode, Microsoft::WRL::ComPtr<ID3D11VertexShader>& outShader) {
		const HRESULT result = device->CreateVertexShader(
			bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &outShader);
		if (SUCCEEDED(result))
			return {};
		return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
			GraphicsErrorCode::ResourceCreationFailed, "vertex shader 생성",
			"DirectX 11 vertex shader를 만들지 못했습니다", result, true));
	}

	GraphicsResult<void> Dx11ShaderProgram::CreatePixelShader(ID3D11Device* device,
		ID3DBlob* bytecode, Microsoft::WRL::ComPtr<ID3D11PixelShader>& outShader) {
		const HRESULT result = device->CreatePixelShader(
			bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &outShader);
		if (SUCCEEDED(result))
			return {};
		return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
			GraphicsErrorCode::ResourceCreationFailed, "pixel shader 생성",
			"DirectX 11 pixel shader를 만들지 못했습니다", result, true));
	}

	GraphicsResult<void> Dx11ShaderProgram::CreateInputLayout(ID3D11Device* device,
		ID3DBlob* vertexBytecode, const std::span<const D3D11_INPUT_ELEMENT_DESC> inputElements,
		Microsoft::WRL::ComPtr<ID3D11InputLayout>& outInputLayout) {
		const HRESULT result = device->CreateInputLayout(
			inputElements.data(), static_cast<UINT>(inputElements.size()),
			vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(),
			&outInputLayout);
		if (SUCCEEDED(result))
			return {};
		return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
			GraphicsErrorCode::ResourceCreationFailed, "input layout 생성",
			"DirectX 11 input layout을 만들지 못했습니다", result, true));
	}

	GraphicsResult<void> Dx11ShaderProgram::Initialize(ID3D11Device* device,
		const ShaderProgramDefinition& program,
		const std::span<const D3D11_INPUT_ELEMENT_DESC> inputElements) {
		if (device == nullptr || program.shaderPath.empty()
			|| program.vertexEntry.empty() || program.pixelEntry.empty()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidArgument, "셰이더 프로그램 초기화",
				"DirectX 11 device, 셰이더 경로 또는 진입점이 올바르지 않습니다"));
		}
		Microsoft::WRL::ComPtr<ID3DBlob> vertexBytecode;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelBytecode;
		std::string error;
		if (!D3DCompilerHlslCompiler::CompileFile(program.shaderPath,
			program.vertexEntry.c_str(), "vs_5_0", vertexBytecode, error)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::EffectConfigurationFailed, "vertex shader 컴파일",
				error.empty() ? "DirectX 11 vertex shader를 컴파일하지 못했습니다" : std::move(error)));
		}
		if (!D3DCompilerHlslCompiler::CompileFile(program.shaderPath,
			program.pixelEntry.c_str(), "ps_5_0", pixelBytecode, error)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::EffectConfigurationFailed, "pixel shader 컴파일",
				error.empty() ? "DirectX 11 pixel shader를 컴파일하지 못했습니다" : std::move(error)));
		}
		Microsoft::WRL::ComPtr<ID3D11VertexShader> newVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> newPixelShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> newInputLayout;
		auto result = CreateVertexShader(device, vertexBytecode.Get(), newVertexShader);
		if (!result)
			return result;
		result = CreatePixelShader(device, pixelBytecode.Get(), newPixelShader);
		if (!result)
			return result;
		if (!inputElements.empty()) {
			result = CreateInputLayout(
				device, vertexBytecode.Get(), inputElements, newInputLayout);
			if (!result)
				return result;
		}
		vertexShader.Swap(newVertexShader);
		pixelShader.Swap(newPixelShader);
		inputLayout.Swap(newInputLayout);
		return {};
	}
}
