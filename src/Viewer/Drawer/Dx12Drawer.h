#pragma once

#include "Viewer/Drawer/Drawer.h"

#include <glm/glm.hpp>

namespace Chrivent {
	class Dx12Instance;

	// D3D12 명령으로 모델의 각 렌더링 패스를 기록한다.
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
		// 포스트 프로세스용 단일 샘플 depth에 DX12 모델 geometry를 기록한다.
		void DrawDepthOnly() override;

	public:
		~Dx12Drawer() override = default;

		explicit Dx12Drawer(const Dx12Instance& sourceInstance);
	};
}
