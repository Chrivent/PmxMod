#pragma once

#include "Viewer/Instance/Instance.h"

#include <vector>
#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
    struct Dx11Texture;
    struct Dx11Material;
    class Dx11Drawer;
    class Dx11Viewer;

    // 한 모델의 D3D11 버퍼, 재질과 descriptor 상태를 관리한다.
    class Dx11Instance : public Instance {
        // 모델 geometry 데이터를 DX11 vertex/index buffer로 생성한다.
        bool CreateGeometryBuffers();
        // 패스별 constant buffer를 생성한다.
        bool CreateConstantBuffers();
        // 모델 material 정보를 DX11 material 캐시와 texture 리소스로 변환한다.
        void LoadMaterials();
        // DX11 상수 버퍼 크기 규칙에 맞춰 16바이트 정렬 버퍼를 생성한다.
        template<typename T>
        static HRESULT CreateBuffer(ID3D11Device* device, Microsoft::WRL::ComPtr<ID3D11Buffer>& out) {
            constexpr UINT bytes = (sizeof(T) + 15u) / 16u * 16u;
            const CD3D11_BUFFER_DESC desc(bytes, D3D11_BIND_CONSTANT_BUFFER);
            return device->CreateBuffer(&desc, nullptr, out.GetAddressOf());
        }
        
	protected:
		// DX11 모델 GPU 리소스를 초기 상태로 되돌린다.
		void ResetRendererResources() override;
		// DX11 모델 리소스를 생성하고 인스턴스를 초기화한다.
		bool SetupRenderer() override;

    public:
        Dx11Viewer*                             viewer = nullptr;
        std::vector<Dx11Material>               materials;
        Microsoft::WRL::ComPtr<ID3D11Buffer>    vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	indexBuffer;
        DXGI_FORMAT                             indexBufferFormat = DXGI_FORMAT_R16_UINT;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	vsConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	psConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>	sceneSurfaceConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	edgeVsConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	edgePsConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	gsVsConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	gsPsConstantBuffer;

		explicit Dx11Instance(Dx11Viewer& sourceViewer);
        
		// 모델의 갱신된 버텍스 데이터를 DX11 버퍼에 반영한다.
        void Upload() const override;
    };
}
