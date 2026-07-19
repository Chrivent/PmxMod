#pragma once

#include "Viewer/DrawResource/Dx11ModelResources.h"
#include "Viewer/Instance/Instance.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
	class Dx11Drawer;
	class Dx11DrawContext;
	class Dx11Device;
	class Dx11TextureCache;

	// 한 모델의 D3D11 버퍼, 재질과 descriptor 상태를 관리한다.
	class Dx11Instance : public Instance {
		const Dx11Device& device;
		Dx11TextureCache& textureCache;
		Dx11DrawContext& drawContext;
		Dx11ModelResources modelResources;

		// 모델 geometry 데이터를 DX11 vertex/index buffer로 생성한다.
		GraphicsError::Result<void> CreateGeometryBuffers();
		// 패스별 constant buffer를 생성한다.
		GraphicsError::Result<void> CreateConstantBuffers();
		// 모델 material 정보를 DX11 material 캐시와 texture 리소스로 변환한다.
		GraphicsError::Result<void> LoadMaterials();
		// DX11 상수 버퍼 크기 규칙에 맞춰 요청한 크기의 16바이트 정렬 버퍼를 생성한다.
		static HRESULT CreateConstantBuffer(ID3D11Device* device, size_t size,
			Microsoft::WRL::ComPtr<ID3D11Buffer>& out);

	protected:
		// DX11 모델 GPU 리소스를 초기 상태로 되돌린다.
		void ResetRendererResources() override;
		// DX11 모델 리소스를 생성하고 인스턴스를 초기화한다.
		GraphicsError::Result<void> SetupRenderer() override;
		// 모델의 갱신된 버텍스 데이터를 DX11 버퍼에 반영한다.
		GraphicsError::Result<void> UploadCore() override;

	public:
		Dx11Instance(const Dx11Device& sourceDevice, Dx11TextureCache& sourceTextureCache,
			Dx11DrawContext& sourceDrawContext);
	};
}
