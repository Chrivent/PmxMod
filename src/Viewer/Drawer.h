#pragma once

namespace Chrivent {
	class Drawer {
	protected:
		// 일반 메시 패스를 그린다.
		virtual void DrawModel() = 0;
		// 엣지 패스를 그린다.
		virtual void DrawEdge() = 0;
		// 지면 그림자 패스를 그린다.
		virtual void DrawGroundShadow() = 0;

	public:
		virtual ~Drawer();

		// 현재 드로어가 가진 패스 순서대로 화면에 그린다.
		void Draw();
	};
}
