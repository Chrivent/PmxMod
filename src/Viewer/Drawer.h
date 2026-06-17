#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	class Drawer {
	protected:
		// 렌더러별 clip space 보정 행렬을 반환한다.
		virtual const glm::mat4& ClipMatrix() const;
		// 지면 그림자 투영에 사용할 평면 그림자 행렬을 만든다.
		static glm::mat4 BuildGroundShadowMatrix(const glm::vec3& lightDir);

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
