#pragma once

#include "../Drawer.h"

namespace Chrivent {
	class GlfwInstance;

	class GlfwDrawer : public Drawer {
		const GlfwInstance& instance;

	protected:
		// 일반 메시 패스를 OpenGL로 렌더링한다.
		void DrawModel() const override;
		// 엣지 패스를 OpenGL로 렌더링한다.
		void DrawEdge() const override;
		// 지면 그림자 패스를 OpenGL로 렌더링한다.
		void DrawGroundShadow() const override;

	public:
		explicit GlfwDrawer(const GlfwInstance& sourceInstance);
		~GlfwDrawer() override = default;
	};
}
