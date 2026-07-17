#pragma once

#include "Viewer/Shader/OpenGlShader.h"
#include "Viewer/Shader/SceneShaderRuntimeContract.h"

namespace Chrivent {
	// OpenGL 장면 렌더링에 사용하는 패스별 셰이더 프로그램을 생성하고 소유한다.
	class OpenGlPipeline {
		OpenGlModelShader modelShader;
		OpenGlEdgeShader edgeShader;
		OpenGlGroundShadowShader groundShadowShader;
		OpenGlDepthOnlyShader depthOnlyShader;
		OpenGlSceneVelocityShader sceneVelocityShader;

	public:
		const OpenGlModelShader& GetModelShader() const { return modelShader; }
		const OpenGlEdgeShader& GetEdgeShader() const { return edgeShader; }
		const OpenGlGroundShadowShader& GetGroundShadowShader() const { return groundShadowShader; }
		const OpenGlDepthOnlyShader& GetDepthOnlyShader() const { return depthOnlyShader; }
		const OpenGlSceneVelocityShader& GetSceneVelocityShader() const { return sceneVelocityShader; }

		// 내부 장면 패스 계약으로 OpenGL 셰이더 프로그램들을 초기화한다.
		bool Initialize(const BuiltInShaderPasses& builtInPasses,
			const SceneInputShaderPasses& sceneInputPasses);
	};
}
