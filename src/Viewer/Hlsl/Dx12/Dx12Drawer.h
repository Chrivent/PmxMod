#pragma once

#include "../../Drawer.h"

#include <glm/glm.hpp>

namespace Chrivent {
	class Dx12Instance;

	class Dx12Drawer : public Drawer {
		const Dx12Instance& instance;

	protected:
		// DirectX depth range로 맞추는 clip 보정 행렬을 반환한다.
		const glm::mat4& ClipMatrix() const override;
		// 일반 메시 패스를 DX12로 렌더링한다.
		void DrawModel() override;
		// 엣지 패스를 DX12로 렌더링한다.
		void DrawEdge() override;
		// 지면 그림자 패스를 DX12로 렌더링한다.
		void DrawGroundShadow() override;

	public:
		~Dx12Drawer() override = default;

		explicit Dx12Drawer(const Dx12Instance& sourceInstance);
	};
}
