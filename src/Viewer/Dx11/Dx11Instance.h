#pragma once

#include "../Instance.h"

#include <vector>
#include <d3d11.h>
#include <wrl/client.h>
#include <glm/glm.hpp>

namespace Chrivent {
    struct Dx11Texture;
    struct Dx11Material;
    class Dx11Viewer;

    class Dx11Instance : public Instance {
    protected:
        // 일반 메시 패스를 DX11로 렌더링한다.
        void DrawModel() const override;
        // 엣지 패스를 DX11로 렌더링한다.
        void DrawEdge() const override;
        // 지면 그림자 패스를 DX11로 렌더링한다.
        void DrawGroundShadow() const override;
        // 모델 버텍스 데이터를 매 프레임 갱신할 동적 버퍼 설명자를 만든다.
        static D3D11_BUFFER_DESC MakeVertexBufferDesc(size_t vertexCount);
        // 모델 인덱스 데이터를 한 번 업로드할 immutable 버퍼 설명자를 만든다.
        static D3D11_BUFFER_DESC MakeIndexBufferDesc(size_t indexBytes);
        // 상수 버퍼 크기를 16바이트 정렬해 생성한다.
        template<typename T>
        static HRESULT CreateBuffer(ID3D11Device* device, Microsoft::WRL::ComPtr<ID3D11Buffer>& out) {
            const UINT bytes = static_cast<UINT>(sizeof(T) + 15u & ~15u);
            const CD3D11_BUFFER_DESC desc(bytes, D3D11_BIND_CONSTANT_BUFFER);
            return device->CreateBuffer(&desc, nullptr, out.GetAddressOf());
        }
        // 텍스처 유무에 따라 실제 SRV 또는 더미 SRV를 픽셀 셰이더 슬롯에 바인딩한다.
        void BindTexture(UINT slot, const Dx11Texture& tex, ID3D11SamplerState* sampler,
            int modeIfPresent, int& outMode, glm::vec4& outMul, glm::vec4& outAdd,
            const glm::vec4& mulIn, const glm::vec4& addIn) const;
        // OpenGL 스타일 clip space를 DX11 depth range로 변환하는 행렬을 반환한다.
        static const glm::mat4& DxClipMatrix();

        Dx11Viewer*                             viewer = nullptr;
        std::vector<Dx11Material>               materials;
        Microsoft::WRL::ComPtr<ID3D11Buffer>    vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	indexBuffer;
        DXGI_FORMAT                             indexBufferFormat = DXGI_FORMAT_R8_UINT;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	vsConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	psConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	edgeVsConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	edgeSizeVsConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	edgePsConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	gsVsConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	gsPsConstantBuffer;
    
    public:
        // 모델 데이터를 DX11 버퍼와 재질 리소스로 업로드한다.
        bool Setup(Viewer& baseViewer) override;
        // 모델의 갱신된 버텍스 데이터를 DX11 버퍼에 반영한다.
        void Update() const override;
    };
}
