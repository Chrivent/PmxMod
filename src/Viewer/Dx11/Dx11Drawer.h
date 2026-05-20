#pragma once

#include "../Drawer.h"

namespace Chrivent {
	class Dx11Instance;

	class Dx11Drawer : public Drawer {
		const Dx11Instance& instance;

	protected:
		// 일반 메시 패스를 DX11로 렌더링한다.
		void DrawModel() const override;
		// 엣지 패스를 DX11로 렌더링한다.
		void DrawEdge() const override;
		// 지면 그림자 패스를 DX11로 렌더링한다.
		void DrawGroundShadow() const override;

	public:
		explicit Dx11Drawer(const Dx11Instance& sourceInstance);
		~Dx11Drawer() override = default;
	};
}
