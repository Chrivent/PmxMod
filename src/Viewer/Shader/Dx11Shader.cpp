#include "Viewer/Shader/Dx11Shader.h"

#include "Viewer/Shader/D3DCompilerHlslCompiler.h"
#include "Viewer/Geometry/ViewerGeometry.h"

#include <cstddef>
#include <utility>

namespace Chrivent {
	GraphicsResult<void> Dx11Shader::CreateVertexShader(ID3D11Device* device, ID3DBlob* bytecode,
		Microsoft::WRL::ComPtr<ID3D11VertexShader>& outShader) {
		const HRESULT result = device->CreateVertexShader(
			bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &outShader);
		if (SUCCEEDED(result))
			return {};
		return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
			GraphicsErrorCode::ResourceCreationFailed, "vertex shader 생성",
			"DirectX 11 vertex shader를 만들지 못했습니다", result, true));
	}

	GraphicsResult<void> Dx11Shader::CreatePixelShader(ID3D11Device* device, ID3DBlob* bytecode,
		Microsoft::WRL::ComPtr<ID3D11PixelShader>& outShader) {
		const HRESULT result = device->CreatePixelShader(
			bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &outShader);
		if (SUCCEEDED(result))
			return {};
		return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
			GraphicsErrorCode::ResourceCreationFailed, "pixel shader 생성",
			"DirectX 11 pixel shader를 만들지 못했습니다", result, true));
	}

	GraphicsResult<void> Dx11Shader::CreateInputLayout(ID3D11Device* device, ID3DBlob* vertexBytecode,
		const std::span<const D3D11_INPUT_ELEMENT_DESC> inputElements,
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

	GraphicsResult<void> Dx11Shader::Initialize(ID3D11Device* device, const std::filesystem::path& file,
		const std::span<const D3D11_INPUT_ELEMENT_DESC> inputElements,
		const char* vertexEntry, const char* pixelEntry) {
		if (device == nullptr || file.empty() || vertexEntry == nullptr || pixelEntry == nullptr) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidArgument, "셰이더 초기화",
				"DirectX 11 device, 셰이더 경로 또는 진입점이 올바르지 않습니다"));
		}
		Microsoft::WRL::ComPtr<ID3DBlob> vertexBytecode;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelBytecode;
		std::string error;
		if (!D3DCompilerHlslCompiler::CompileFile(
			file, vertexEntry, "vs_5_0", vertexBytecode, error)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::EffectConfigurationFailed, "vertex shader 컴파일",
				error.empty() ? "DirectX 11 vertex shader를 컴파일하지 못했습니다" : std::move(error)));
		}
		if (!D3DCompilerHlslCompiler::CompileFile(
			file, pixelEntry, "ps_5_0", pixelBytecode, error)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::EffectConfigurationFailed, "pixel shader 컴파일",
				error.empty() ? "DirectX 11 pixel shader를 컴파일하지 못했습니다" : std::move(error)));
		}
		auto result = CreateVertexShader(device, vertexBytecode.Get(), vertexShader);
		if (!result)
			return result;
		result = CreatePixelShader(device, pixelBytecode.Get(), pixelShader);
		if (!result)
			return result;
		if (!inputElements.empty())
			return CreateInputLayout(device, vertexBytecode.Get(), inputElements, inputLayout);
		return {};
	}

	GraphicsResult<void> Dx11ModelShader::Initialize(
		ID3D11Device* device, const ShaderProgramDefinition& program) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, program.shaderPath, inputElements,
			program.vertexEntry.c_str(), program.pixelEntry.c_str());
	}

	GraphicsResult<void> Dx11EdgeShader::Initialize(
		ID3D11Device* device, const ShaderProgramDefinition& program) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, program.shaderPath, inputElements,
			program.vertexEntry.c_str(), program.pixelEntry.c_str());
	}

	GraphicsResult<void> Dx11GroundShadowShader::Initialize(
		ID3D11Device* device, const ShaderProgramDefinition& program) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, program.shaderPath, inputElements,
			program.vertexEntry.c_str(), program.pixelEntry.c_str());
	}

	GraphicsResult<void> Dx11SceneDepthShader::Initialize(
		ID3D11Device* device, const ShaderProgramDefinition& program) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ViewerVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		return Dx11Shader::Initialize(device, program.shaderPath, inputElements,
			program.vertexEntry.c_str(), program.pixelEntry.c_str());
	}

	GraphicsResult<void> Dx11SceneVelocityShader::Initialize(
		ID3D11Device* device, const ShaderProgramDefinition& program) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, previousPosition), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ViewerVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		return Dx11Shader::Initialize(device, program.shaderPath, inputElements,
			program.vertexEntry.c_str(), program.pixelEntry.c_str());
	}

	GraphicsResult<void> Dx11PostProcessShader::Initialize(
		ID3D11Device* device, const std::filesystem::path& file,
		const char* vertexEntry, const char* pixelEntry) {
		return Dx11Shader::Initialize(device, file, {}, vertexEntry, pixelEntry);
	}
}
