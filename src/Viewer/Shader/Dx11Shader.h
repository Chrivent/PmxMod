#pragma once

#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/ShaderProgramDefinition.h"

#include <d3d11.h>
#include <span>
#include <wrl/client.h>

namespace Chrivent {
	// D3D11 vertex/pixel shader와 선택적인 input layout을 하나의 프로그램 단위로 관리한다.
	class Dx11ShaderProgram {
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

		// 컴파일된 vertex bytecode로 D3D11 vertex shader 객체를 생성한다.
		static GraphicsResult<void> CreateVertexShader(ID3D11Device* device, ID3DBlob* bytecode,
			Microsoft::WRL::ComPtr<ID3D11VertexShader>& outShader);
		// 컴파일된 pixel bytecode로 D3D11 pixel shader 객체를 생성한다.
		static GraphicsResult<void> CreatePixelShader(ID3D11Device* device, ID3DBlob* bytecode,
			Microsoft::WRL::ComPtr<ID3D11PixelShader>& outShader);
		// 컴파일된 vertex bytecode와 입력 요소 정보로 D3D11 input layout을 생성한다.
		static GraphicsResult<void> CreateInputLayout(ID3D11Device* device, ID3DBlob* vertexBytecode,
			std::span<const D3D11_INPUT_ELEMENT_DESC> inputElements,
			Microsoft::WRL::ComPtr<ID3D11InputLayout>& outInputLayout);

	public:
		ID3D11VertexShader* GetVertexShader() const { return vertexShader.Get(); }
		ID3D11PixelShader* GetPixelShader() const { return pixelShader.Get(); }
		ID3D11InputLayout* GetInputLayout() const { return inputLayout.Get(); }

		// HLSL 프로그램을 컴파일하고 지정한 입력 요소에 맞는 D3D11 객체로 교체한다.
		GraphicsResult<void> Initialize(ID3D11Device* device,
			const ShaderProgramDefinition& program,
			std::span<const D3D11_INPUT_ELEMENT_DESC> inputElements = {});
	};
}
