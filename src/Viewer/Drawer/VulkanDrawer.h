#pragma once

#include "Viewer/Buffer/VulkanDynamicBufferRing.h"
#include "Viewer/Drawer/Drawer.h"

#include <glm/glm.hpp>

namespace Chrivent {
	class VulkanInstance;
	class VulkanDrawContext;
	struct VulkanModelResources;

	// Vulkan 명령으로 모델의 각 렌더링 패스를 기록한다.
	class VulkanDrawer final : public Drawer {
		const VulkanInstance& instance;
		VulkanModelResources& resources;
		VulkanDrawContext& drawContext;

		// Vulkan 동적 버퍼에서 상수 범위를 예약하고 데이터를 기록한다.
		static DynamicBufferError::Result<UploadSlice> UploadConstants(
			VulkanDynamicBufferRing& ring, size_t alignment, const void* data, size_t size);

	protected:
		// 새 프레임용 Vulkan 동적 uniform buffer ring 상태를 초기화한다.
		void BeginDrawFrame() override;
		// GL/DX와 같은 화면 좌표 및 깊이 범위로 맞추는 Vulkan clip 보정 행렬을 반환한다.
		const glm::mat4& ClipMatrix() const override;
		// 일반 메시 패스를 Vulkan으로 렌더링한다.
		GraphicsError::Result<void> DrawModel() override;
		// 엣지 패스를 Vulkan으로 렌더링한다.
		GraphicsError::Result<void> DrawEdge() override;
		// 지면 그림자 패스를 Vulkan으로 렌더링한다.
		GraphicsError::Result<void> DrawGroundShadow() override;
		// 포스트 프로세스용 단일 샘플 depth에 Vulkan 모델 geometry를 기록한다.
		GraphicsError::Result<void> DrawSceneInputs() override;

	public:
		VulkanDrawer(const VulkanInstance& sourceInstance, VulkanModelResources& sourceResources,
			VulkanDrawContext& sourceDrawContext);
	};
}
