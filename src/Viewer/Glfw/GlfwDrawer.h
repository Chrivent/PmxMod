#pragma once

#include "../Drawer.h"

#include <glad/glad.h>

namespace Chrivent {
	struct GlfwInstanceInfo;

	class GlfwDrawer : public Drawer {
		const GlfwInstanceInfo& info;

	protected:
		// uniform buffer에 상수 데이터를 갱신하고 지정한 binding에 연결한다.
		static void UpdateUniformBuffer(GLuint buffer, GLuint binding, const void* data, size_t size);

		// 일반 메시 패스를 OpenGL로 렌더링한다.
		void DrawModel() const override;
		// 엣지 패스를 OpenGL로 렌더링한다.
		void DrawEdge() const override;
		// 지면 그림자 패스를 OpenGL로 렌더링한다.
		void DrawGroundShadow() const override;

	public:
		explicit GlfwDrawer(const GlfwInstanceInfo& sourceInfo);
		~GlfwDrawer() override = default;
	};
}
