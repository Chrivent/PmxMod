#pragma once

#include "../Drawer.h"

namespace Chrivent {
	struct Dx12InstanceInfo;

	class Dx12Drawer : public Drawer {
		const Dx12InstanceInfo& info;

	protected:
		// 일반 메시 패스를 DX12로 렌더링한다.
		void DrawModel() override;
		// 엣지 패스를 DX12로 렌더링한다.
		void DrawEdge() override;
		// 지면 그림자 패스를 DX12로 렌더링한다.
		void DrawGroundShadow() override;

	public:
		explicit Dx12Drawer(const Dx12InstanceInfo& sourceInfo);
		~Dx12Drawer() override = default;
	};
}
