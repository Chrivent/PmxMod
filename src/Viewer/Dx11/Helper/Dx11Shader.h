#pragma once

#include "Viewer/Shader/ShaderPackage.h"

#include <filesystem>
#include <d3d11.h>
#include <span>
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
			std::span<const D3D11_INPUT_ELEMENT_DESC> inputElements,
			Microsoft::WRL::ComPtr<ID3D11InputLayout>& outInputLayout);
		// HLSL 파일을 컴파일하고 vertex/pixel shader와 input layout을 생성한다.
		bool Initialize(ID3D11Device* device, const std::filesystem::path& file,
			std::span<const D3D11_INPUT_ELEMENT_DESC> inputElements,
			const char* vertexEntry = "VSMain", const char* pixelEntry = "PSMain");
	};

	struct Dx11ModelShader : Dx11Shader {
		// 모델 렌더링용 HLSL shader 묶음을 생성한다.
		bool Initialize(ID3D11Device* device, const EffectPassDefinition& pass);
	};

	struct Dx11EdgeShader : Dx11Shader {
		// 엣지 렌더링용 HLSL shader 묶음을 생성한다.
		bool Initialize(ID3D11Device* device, const EffectPassDefinition& pass);
	};

	struct Dx11GroundShadowShader : Dx11Shader {
		// 지면 그림자 렌더링용 HLSL shader 묶음을 생성한다.
		bool Initialize(ID3D11Device* device, const EffectPassDefinition& pass);
	};

	struct Dx11SceneVelocityShader : Dx11Shader {
		// 현재/이전 스키닝 위치를 입력받는 DX11 장면 속도 셰이더를 생성한다.
		bool Initialize(ID3D11Device* device, const std::filesystem::path& file);
	};

	struct Dx11PostProcessShader : Dx11Shader {
		// 입력 레이아웃이 없는 풀스크린 포스트 프로세스 셰이더를 생성한다.
		bool Initialize(ID3D11Device* device, const std::filesystem::path& file, const char* vertexEntry, const char* pixelEntry);
	};
}
