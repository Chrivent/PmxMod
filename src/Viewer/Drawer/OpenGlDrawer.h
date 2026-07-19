#pragma once

#include "Viewer/Drawer/Drawer.h"
#include "Viewer/Buffer/OpenGlDynamicBufferRing.h"

#include <glad/glad.h>

namespace Chrivent {
	class OpenGlInstance;
	class OpenGlDrawContext;
	struct OpenGlModelResources;

	// OpenGL 명령으로 모델의 각 렌더링 패스를 실행한다.
	class OpenGlDrawer : public Drawer {
		const OpenGlInstance& instance;
		OpenGlModelResources& resources;
		OpenGlDrawContext& drawContext;

		// uniform buffer ring에 상수 데이터를 기록하고 지정한 binding에 연결한다.
		DynamicBufferError::Result<void> UpdateUniformBuffer(
			OpenGlDynamicBufferRing& ring, GLuint binding, const void* data, size_t size) const;

	protected:
		// 새 프레임용 OpenGL 업로드 링 버퍼 상태를 초기화한다.
		void BeginDrawFrame() override;
		// 일반 메시 패스를 OpenGL로 렌더링한다.
		GraphicsError::Result<void> DrawModel() override;
		// 엣지 패스를 OpenGL로 렌더링한다.
		GraphicsError::Result<void> DrawEdge() override;
		// 지면 그림자 패스를 OpenGL로 렌더링한다.
		GraphicsError::Result<void> DrawGroundShadow() override;
		// 포스트 프로세스용 단일 샘플 depth에 OpenGL 모델 geometry를 기록한다.
		GraphicsError::Result<void> DrawSceneInputs() override;

	public:
		OpenGlDrawer(const OpenGlInstance& sourceInstance, OpenGlModelResources& sourceResources,
			OpenGlDrawContext& sourceDrawContext);
	};
}
