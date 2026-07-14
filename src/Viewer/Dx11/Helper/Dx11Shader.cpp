#include "Viewer/Dx11/Helper/Dx11Shader.h"

#include "Viewer/Shader/ShaderCompiler.h"
#include "Viewer/ViewerGeometry.h"

#include <cstddef>
#include <iostream>

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
		const char* vertexEntry, const char* pixelEntry) {
		Microsoft::WRL::ComPtr<ID3DBlob> vertexBytecode;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelBytecode;
		std::string error;
		if (!ShaderCompiler::CompileFile(file, vertexEntry, "vs_5_0", vertexBytecode, error)) {
			std::cerr << error << '\n';
			return false;
		}
		if (!ShaderCompiler::CompileFile(file, pixelEntry, "ps_5_0", pixelBytecode, error)) {
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
		if (!inputElements.empty() && !CreateInputLayout(device, vertexBytecode.Get(), inputElements, inputLayout)) {
			std::cerr << "Failed to create DX11 input layout: " << file.string() << '\n';
			return false;
		}
		return true;
	}

	bool Dx11ModelShader::Initialize(ID3D11Device* device, const EffectPassDefinition& pass) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, pass.shaderPath, inputElements,
			pass.vertexEntry.c_str(), pass.pixelEntry.c_str());
	}

	bool Dx11EdgeShader::Initialize(ID3D11Device* device, const EffectPassDefinition& pass) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, pass.shaderPath, inputElements,
			pass.vertexEntry.c_str(), pass.pixelEntry.c_str());
	}

	bool Dx11GroundShadowShader::Initialize(ID3D11Device* device, const EffectPassDefinition& pass) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		return Dx11Shader::Initialize(device, pass.shaderPath, inputElements,
			pass.vertexEntry.c_str(), pass.pixelEntry.c_str());
	}

	bool Dx11SceneVelocityShader::Initialize(ID3D11Device* device, const std::filesystem::path& file) {
		constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, previousPosition), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		return Dx11Shader::Initialize(device, file, inputElements, "VSMain", "PSMainInvertedY");
	}

	bool Dx11PostProcessShader::Initialize(ID3D11Device* device, const std::filesystem::path& file, const char* vertexEntry, const char* pixelEntry) {
		return Dx11Shader::Initialize(device, file, {}, vertexEntry, pixelEntry);
	}
}
