#pragma once

#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/OpenGlShader.h"
#include "Viewer/Shader/SceneShaderRuntimeContract.h"

namespace Chrivent {
	// OpenGL 장면 렌더링에 사용하는 패스별 셰이더 프로그램을 생성하고 소유한다.
	class OpenGlPipeline {
		OpenGlSceneShaderProgram modelProgram;
		OpenGlSceneShaderProgram edgeProgram;
		OpenGlSceneShaderProgram groundShadowProgram;
		OpenGlSceneShaderProgram sceneDepthProgram;
		OpenGlSceneShaderProgram sceneVelocityProgram;

	public:
		// 내부 장면 패스 계약으로 OpenGL 셰이더 프로그램들을 초기화한다.
		GraphicsError::Result<void> Initialize(const SceneShaderRuntimeContract& shaderContract);
		// 모델 표면 프로그램을 바인딩한다.
		void BindModel() const;
		// 엣지 프로그램을 바인딩한다.
		void BindEdge() const;
		// 지면 그림자 프로그램을 바인딩한다.
		void BindGroundShadow() const;
		// 장면 depth 프로그램을 바인딩한다.
		void BindSceneDepth() const;
		// 장면 velocity 프로그램을 바인딩한다.
		void BindSceneVelocity() const;
	};
}
