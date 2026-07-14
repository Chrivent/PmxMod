#pragma once

#include "Viewer/Drawer/Drawer.h"
#include "Viewer/Buffer/GlfwDynamicBufferRing.h"

#include <glad/glad.h>

namespace Chrivent {
	class GlfwInstance;

	class GlfwDrawer : public Drawer {
		GlfwInstance& instance;

		// 새 프레임용 OpenGL 업로드 링 버퍼 상태를 초기화한다.
		void BeginDynamicBufferFrame() const;
		// uniform buffer ring에 상수 데이터를 기록하고 지정한 binding에 연결한다.
		bool UpdateUniformBuffer(GlfwDynamicBufferRing& ring, GLuint binding, const void* data, size_t size) const;

	protected:
		// 일반 메시 패스를 OpenGL로 렌더링한다.
		void DrawModel() override;
		// 엣지 패스를 OpenGL로 렌더링한다.
		void DrawEdge() override;
		// 지면 그림자 패스를 OpenGL로 렌더링한다.
		void DrawGroundShadow() override;
		// 포스트 프로세스용 단일 샘플 depth에 OpenGL 모델 geometry를 기록한다.
		void DrawDepthOnly() override;

	public:
		~GlfwDrawer() override = default;

		explicit GlfwDrawer(GlfwInstance& sourceInstance);
	};
}
