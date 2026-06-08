#pragma once

#include "../Drawer.h"

#include <glm/glm.hpp>

namespace Chrivent {
	struct Dx12InstanceInfo;

	class Dx12Drawer : public Drawer {
		const Dx12InstanceInfo& info;

		// OpenGL 스타일 clip space를 DirectX depth range로 변환하는 행렬을 반환한다.
		static const glm::mat4& ClipMatrix();

	protected:
		// 일반 메시 패스를 DX12로 렌더링한다.
		void DrawModel() override;
		// 엣지 패스를 DX12로 렌더링한다.
		void DrawEdge() override;
		// 지면 그림자 패스를 DX12로 렌더링한다.
		void DrawGroundShadow() override;

	public:
		~Dx12Drawer() override = default;

		explicit Dx12Drawer(const Dx12InstanceInfo& sourceInfo);
	};
}
