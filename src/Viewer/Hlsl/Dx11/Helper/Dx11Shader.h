#pragma once

#include <filesystem>
#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	struct Dx11Shader {
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

		// 컴파일된 vertex bytecode로 DX11 vertex shader 객체를 생성한다.
		static bool CreateVertexShader(ID3D11Device* device, ID3DBlob* bytecode, Microsoft::WRL::ComPtr<ID3D11VertexShader>& outShader);
		// 컴파일된 pixel bytecode로 DX11 pixel shader 객체를 생성한다.
		static bool CreatePixelShader(ID3D11Device* device, ID3DBlob* bytecode, Microsoft::WRL::ComPtr<ID3D11PixelShader>& outShader);
		// 컴파일된 vertex bytecode와 입력 요소 정보로 DX11 input layout을 생성한다.
		static bool CreateInputLayout(ID3D11Device* device, ID3DBlob* vertexBytecode,
			const D3D11_INPUT_ELEMENT_DESC* inputElements, UINT inputElementCount,
			Microsoft::WRL::ComPtr<ID3D11InputLayout>& outInputLayout);
		// HLSL 파일을 컴파일하고 vertex/pixel shader와 input layout을 생성한다.
		bool Initialize(ID3D11Device* device, const std::filesystem::path& file,
			const D3D11_INPUT_ELEMENT_DESC* inputElements, UINT inputElementCount,
			const char* vertexEntry = "VSMain", const char* pixelEntry = "PSMain");
	};

	struct Dx11ModelShader : Dx11Shader {
		// 모델 렌더링용 HLSL shader 묶음을 생성한다.
		bool Initialize(ID3D11Device* device, const std::filesystem::path& file);
	};

	struct Dx11EdgeShader : Dx11Shader {
		// 엣지 렌더링용 HLSL shader 묶음을 생성한다.
		bool Initialize(ID3D11Device* device, const std::filesystem::path& file);
	};

	struct Dx11GroundShadowShader : Dx11Shader {
		// 지면 그림자 렌더링용 HLSL shader 묶음을 생성한다.
		bool Initialize(ID3D11Device* device, const std::filesystem::path& file);
	};

	struct Dx11PostProcessShader : Dx11Shader {
		// 입력 레이아웃이 없는 풀스크린 포스트 프로세스 셰이더를 생성한다.
		bool Initialize(ID3D11Device* device, const std::filesystem::path& file, const char* vertexEntry, const char* pixelEntry);
	};
}
