#pragma once

#include "../Drawer.h"

#include <glm/glm.hpp>

namespace Chrivent {
	struct VulkanInstanceInfo;

	class VulkanDrawer : public Drawer {
		const VulkanInstanceInfo& info;

		// GL/DX와 같은 화면 좌표 및 깊이 범위로 맞추는 clip 보정 행렬을 반환한다.
		static const glm::mat4& VulkanClipMatrix();

	protected:
		// 일반 메시 패스를 Vulkan으로 렌더링한다.
		void DrawModel() const override;
		// 엣지 패스를 Vulkan으로 렌더링한다.
		void DrawEdge() const override;
		// 지면 그림자 패스를 Vulkan으로 렌더링한다.
		void DrawGroundShadow() const override;

	public:
		explicit VulkanDrawer(const VulkanInstanceInfo& sourceInfo);
		~VulkanDrawer() override = default;
	};
}
