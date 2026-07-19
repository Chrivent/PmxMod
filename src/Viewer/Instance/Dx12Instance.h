#pragma once

#include "Viewer/DrawResource/Dx12ModelResources.h"
#include "Viewer/Instance/Instance.h"

namespace Chrivent {
	class Dx12Drawer;
	class Dx12DrawContext;
	class Dx12Device;
	class Dx12UploadContext;
	class Dx12TextureCache;
	struct Dx12Texture;

	// 한 모델의 D3D12 버퍼, 재질과 descriptor 상태를 관리한다.
	class Dx12Instance : public Instance {
		const Dx12Device& device;
		Dx12UploadContext& uploadContext;
		Dx12TextureCache& textureCache;
		const Dx12Texture& dummyTexture;
		Dx12DrawContext& drawContext;
		Dx12ModelResources modelResources;

		// 모델 geometry 데이터를 DX12 vertex/index buffer로 업로드한다.
		GraphicsError::Result<void> CreateGeometryBuffers();
		// 패스별 constant buffer를 material 개수에 맞춰 생성한다.
		GraphicsError::Result<void> CreateConstantBuffers();
		// 모델 material 정보를 DX12 material 캐시와 texture descriptor 준비 데이터로 변환한다.
		GraphicsError::Result<void> LoadMaterials();
		// material별 텍스처 SRV descriptor를 생성한다.
		GraphicsError::Result<void> CreateTextureDescriptors();
		
	protected:
		// DX12 모델 GPU 리소스를 초기 상태로 되돌린다.
		void ResetRendererResources() override;
		// DX12 모델 리소스를 생성하고 인스턴스를 초기화한다.
		GraphicsError::Result<void> SetupRenderer() override;
		// 모델의 갱신된 버텍스 데이터를 DX12 리소스에 반영한다.
		GraphicsError::Result<void> UploadCore() override;

	public:
		Dx12Instance(const Dx12Device& sourceDevice,
			Dx12UploadContext& sourceUploadContext, Dx12TextureCache& sourceTextureCache,
			const Dx12Texture& sourceDummyTexture, Dx12DrawContext& sourceDrawContext);
	};
}
