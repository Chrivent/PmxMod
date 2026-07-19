#include "Viewer/Shader/Dx11Shader.h"

#include "Viewer/Shader/D3DCompilerHlslCompiler.h"
#include "Viewer/Geometry/ViewerGeometry.h"

#include <cstddef>

namespace Chrivent {
	bool Dx11Shader::CreateVertexShader(ID3D11Device* device, ID3DBlob* bytecode, Microsoft::WRL::ComPtr<ID3D11VertexShader>& outShader) {
		return SUCCEEDED(device->CreateVertexShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &outShader));
	}

	bool Dx11Shader::CreatePixelShader(ID3D11Device* device, ID3DBlob* bytecode, Microsoft::WRL::ComPtr<ID3D11PixelShader>& outShader) {
		return SUCCEEDED(device->CreatePixelShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &outShader));
	}

	bool Dx11Shader::CreateInputLayout(ID3D11Device* device, ID3DBlob* vertexBytecode,
		const std::span<const D3D11_INPUT_ELEMENT_DESC> inputElements,
		Microsoft::WRL::ComPtr<ID3D11InputLayout>& outInputLayout) {
		return SUCCEEDED(device->CreateInputLayout(inputElements.data(), static_cast<UINT>(inputElements.size()),
			vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(),
			&outInputLayout));
	}

	bool Dx11Shader::Initialize(ID3D11Device* device, const std::filesystem::path& file,
		const std::span<const D3D11_INPUT_ELEMENT_DESC> inputElements,
		std::string& error, const char* vertexEntry, const char* pixelEntry) {
		error.clear();
		Microsoft::WRL::ComPtr<ID3DBlob> vertexBytecode;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelBytecode;
		if (!D3DCompilerHlslCompiler::CompileFile(file, vertexEntry, "vs_5_0", vertexBytecode, error))
			return false;
		if (!D3DCompilerHlslCompiler::CompileFile(file, pixelEntry, "ps_5_0", pixelBytecode, error))
			return false;
		if (!CreateVertexShader(device, vertexBytecode.Get(), vertexShader)) {
			error = "DX11 vertex shader를 만들지 못했습니다: ";
			error += file.string();
			error += " entry=";
			error += vertexEntry;
			return false;
		}
		if (!CreatePixelShader(device, pixelBytecode.Get(), pixelShader)) {
			error = "DX11 pixel shader를 만들지 못했습니다: ";
			error += file.string();
			error += " entry=";
			error += pixelEntry;
			return false;
		}
		if (!inputElements.empty() && !CreateInputLayout(device, vertexBytecode.Get(), inputElements, inputLayout)) {
			error = "DX11 input layout을 만들지 못했습니다: ";
			error += file.string();
			return false;
		}
		return true;
	}

	bool Dx11ModelShader::Initialize(ID3D11Device* device,
		const ShaderProgramDefinition& program, std::string& error) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, program.shaderPath, inputElements, error,
			program.vertexEntry.c_str(), program.pixelEntry.c_str());
	}

	bool Dx11EdgeShader::Initialize(ID3D11Device* device,
		const ShaderProgramDefinition& program, std::string& error) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, program.shaderPath, inputElements, error,
			program.vertexEntry.c_str(), program.pixelEntry.c_str());
	}

	bool Dx11GroundShadowShader::Initialize(ID3D11Device* device,
		const ShaderProgramDefinition& program, std::string& error) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, program.shaderPath, inputElements, error,
			program.vertexEntry.c_str(), program.pixelEntry.c_str());
	}

	bool Dx11SceneDepthShader::Initialize(ID3D11Device* device,
		const ShaderProgramDefinition& program, std::string& error) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ViewerVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		return Dx11Shader::Initialize(device, program.shaderPath, inputElements, error,
			program.vertexEntry.c_str(), program.pixelEntry.c_str());
	}

	bool Dx11SceneVelocityShader::Initialize(ID3D11Device* device,
		const ShaderProgramDefinition& program, std::string& error) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, previousPosition), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ViewerVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		return Dx11Shader::Initialize(device, program.shaderPath, inputElements, error,
			program.vertexEntry.c_str(), program.pixelEntry.c_str());
	}

	bool Dx11PostProcessShader::Initialize(ID3D11Device* device, const std::filesystem::path& file,
		const char* vertexEntry, const char* pixelEntry, std::string& error) {
		return Dx11Shader::Initialize(device, file, {}, error, vertexEntry, pixelEntry);
	}
}
