#pragma once

#include "../Viewer.h"
#include "Dx12TextureCache.h"
#include "Helper/Dx12CommandContext.h"
#include "Helper/Dx12Device.h"
#include "Helper/Dx12Pipeline.h"
#include "Helper/Dx12SwapChain.h"

#include <filesystem>
#include <memory>

namespace Chrivent {
	struct Dx12Material : ViewerMaterial {
		Dx12Texture texture{};

		explicit Dx12Material(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
	};

	struct Dx12ViewerInfo : ViewerInfo {};

	class Dx12Viewer : public Viewer {
		Dx12Device device;
		Dx12SwapChain swapChain;
		Dx12CommandContext commandContext;
		Dx12Pipeline pipeline;
		Dx12TextureCache textureCache;

	public:
		Dx12Viewer();
		~Dx12Viewer() override;

		const Dx12DeviceInfo& GetDeviceInfo() const { return device.GetInfo(); }
		ID3D12GraphicsCommandList* GetCommandList() const { return commandContext.GetCommandList(); }

		// DX12 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
		void ConfigureGlfwHints() override;
		// DX12 렌더러 리소스를 초기화한다.
		bool Setup() override;
		// 창 크기에 맞춰 DX12 스왑체인과 렌더 타깃을 재생성한다.
		bool Resize() override;
		// DX12 프레임 렌더링을 시작한다.
		void BeginFrame() override;
		// DX12 프레임을 제출하고 화면에 표시한다.
		bool EndFrame() override;
		// DX12 모델 인스턴스를 생성한다.
		std::unique_ptr<Instance> CreateInstance() const override;
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 DX12 텍스처로 반환한다.
		Dx12Texture LoadTexture(const std::filesystem::path& texturePath);
	};
}
