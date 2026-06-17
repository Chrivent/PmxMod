#include "Dx11Shader.h"

#include "../../HlslShaderCompiler.h"

#include <iostream>

namespace Chrivent {
	bool Dx11Shader::CreateVertexShader(
		ID3D11Device* device,
		ID3DBlob* bytecode,
		Microsoft::WRL::ComPtr<ID3D11VertexShader>& outShader) {
		return SUCCEEDED(device->CreateVertexShader(
			bytecode->GetBufferPointer(),
			bytecode->GetBufferSize(),
			nullptr,
			&outShader));
	}

	bool Dx11Shader::CreatePixelShader(
		ID3D11Device* device,
		ID3DBlob* bytecode,
		Microsoft::WRL::ComPtr<ID3D11PixelShader>& outShader) {
		return SUCCEEDED(device->CreatePixelShader(
			bytecode->GetBufferPointer(),
			bytecode->GetBufferSize(),
			nullptr,
			&outShader));
	}

	bool Dx11Shader::CreateInputLayout(
		ID3D11Device* device,
		ID3DBlob* vertexBytecode,
		const D3D11_INPUT_ELEMENT_DESC* inputElements,
		const UINT inputElementCount,
		Microsoft::WRL::ComPtr<ID3D11InputLayout>& outInputLayout) {
		return SUCCEEDED(device->CreateInputLayout(
			inputElements,
			inputElementCount,
			vertexBytecode->GetBufferPointer(),
			vertexBytecode->GetBufferSize(),
			&outInputLayout));
	}

	bool Dx11Shader::Initialize(
		ID3D11Device* device,
		const std::filesystem::path& file,
		const D3D11_INPUT_ELEMENT_DESC* inputElements,
		const UINT inputElementCount,
		const char* vertexEntry,
		const char* pixelEntry) {
		Microsoft::WRL::ComPtr<ID3DBlob> vertexBytecode;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelBytecode;
		std::string error;
		if (!HlslShaderCompiler::CompileFile(file, vertexEntry, "vs_5_0", vertexBytecode, error)) {
			std::cerr << error << '\n';
			return false;
		}
		if (!HlslShaderCompiler::CompileFile(file, pixelEntry, "ps_5_0", pixelBytecode, error)) {
			std::cerr << error << '\n';
			return false;
		}
		if (!CreateVertexShader(device, vertexBytecode.Get(), vertexShader)) {
			std::cerr << "Failed to create DX11 vertex shader: " << file.string() << " entry=" << vertexEntry << '\n';
			return false;
		}
		if (!CreatePixelShader(device, pixelBytecode.Get(), pixelShader)) {
			std::cerr << "Failed to create DX11 pixel shader: " << file.string() << " entry=" << pixelEntry << '\n';
			return false;
		}
		if (!CreateInputLayout(device, vertexBytecode.Get(), inputElements, inputElementCount, inputLayout)) {
			std::cerr << "Failed to create DX11 input layout: " << file.string() << '\n';
			return false;
		}
		return true;
	}

	bool Dx11ModelShader::Initialize(ID3D11Device* device, const std::filesystem::path& file) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, file, inputElements, 3);
	}

	bool Dx11EdgeShader::Initialize(ID3D11Device* device, const std::filesystem::path& file) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, file, inputElements, 2);
	}

	bool Dx11GroundShadowShader::Initialize(ID3D11Device* device, const std::filesystem::path& file) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, file, inputElements, 1);
	}
}
