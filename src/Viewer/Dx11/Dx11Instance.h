#pragma once

#include "../Instance.h"

#include <vector>
#include <d3d11.h>
#include <wrl/client.h>
#include <glm/glm.hpp>

namespace Chrivent {
    struct Dx11Texture;
    struct Dx11Material;
    class Dx11Drawer;
    class Dx11Viewer;

    struct Dx11InstanceInfo : InstanceInfo {
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
    };

    class Dx11Instance : public Instance {
    protected:
        Dx11InstanceInfo& GetDx11Info() { return static_cast<Dx11InstanceInfo&>(GetInfo()); }
        const Dx11InstanceInfo& GetDx11Info() const { return static_cast<const Dx11InstanceInfo&>(GetInfo()); }

        // 모델 버텍스 데이터를 매 프레임 갱신할 동적 버퍼 설명자를 만든다.
        static D3D11_BUFFER_DESC MakeVertexBufferDesc(size_t vertexCount);
        // 모델 인덱스 데이터를 한 번 업로드할 immutable 버퍼 설명자를 만든다.
        static D3D11_BUFFER_DESC MakeIndexBufferDesc(size_t indexBytes);
        template<typename T>
        static HRESULT CreateBuffer(ID3D11Device* device, Microsoft::WRL::ComPtr<ID3D11Buffer>& out) {
            const UINT bytes = sizeof(T) + 15u & ~15u;
            const CD3D11_BUFFER_DESC desc(bytes, D3D11_BIND_CONSTANT_BUFFER);
            return device->CreateBuffer(&desc, nullptr, out.GetAddressOf());
        }

    public:
        Dx11Instance();
        
        // 모델 데이터를 DX11 버퍼와 재질 리소스로 업로드한다.
        bool Setup(Viewer& baseViewer) override;
        // 모델의 갱신된 버텍스 데이터를 DX11 버퍼에 반영한다.
        void Update() const override;
    };
}
