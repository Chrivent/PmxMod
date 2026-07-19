#pragma once

#include "Viewer/Texture/TextureCache.h"
#include "Viewer/Command/Dx12UploadContext.h"
#include "Viewer/Device/Dx12Device.h"

#include <filesystem>
#include <optional>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	// 캐시 정보와 D3D12 resource 및 크기와 형식을 보관한다.
	struct Dx12Texture {
		bool hasAlpha = false;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		UINT width = 0;
		UINT height = 0;
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	};

	// 이미지 파일을 D3D12 texture로 업로드하고 공통 키로 재사용한다.
	class Dx12TextureCache : public TextureCache<Dx12Texture> {
		Dx12UploadContext& uploadContext;
		std::vector<TextureKey> pendingTextureKeys;
		bool uploadBatchActive = false;

		// RGBA 픽셀을 DX12 texture resource로 업로드한다.
		GraphicsError::Result<void> UploadRgbaPixels(const Dx12Device& sourceDevice, const unsigned char* pixels,
			UINT width, UINT height, Dx12Texture& texture);
		// 제출하지 못한 batch에서 추가한 texture cache 항목을 제거한다.
		void RollbackUploadBatch();

	public:
		explicit Dx12TextureCache(Dx12UploadContext& sourceUploadContext) :
			uploadContext(sourceUploadContext) {}

		// 여러 texture 업로드를 한 command list로 기록할 batch를 시작한다.
		GraphicsError::Result<void> BeginUploadBatch(const Dx12Device& sourceDevice);
		// 기록한 texture upload batch를 한 번 제출한다.
		GraphicsError::Result<void> SubmitUploadBatch(const Dx12Device& sourceDevice);
		// 제출하지 않은 texture upload batch와 새 cache 항목을 폐기한다.
		void CancelUploadBatch();
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 DX12 리소스로 반환한다.
		GraphicsError::Result<std::optional<Dx12Texture>> Load(
			const Dx12Device& sourceDevice, const std::filesystem::path& texturePath);
		// 텍스처가 없는 material에 바인딩할 흰색 DX12 텍스처를 생성한다.
		GraphicsError::Result<Dx12Texture> CreateWhiteTexture(const Dx12Device& sourceDevice);
	};
}
