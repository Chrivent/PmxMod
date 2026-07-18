#pragma once

#include "Viewer/Shader/OpenGlShader.h"
#include "Viewer/Shader/SceneShaderRuntimeContract.h"

namespace Chrivent {
	// OpenGL 장면 패스의 vertex attribute 위치를 API 리소스 생성에 필요한 값으로 묶는다.
	struct OpenGlSceneAttributeLocations {
		GLint model[3]{};
		GLint edge[2]{};
		GLint groundShadow[1]{};
		GLint depth[2]{};
		GLint velocity[3]{};
	};

	// OpenGL 장면 렌더링에 사용하는 패스별 셰이더 프로그램을 생성하고 소유한다.
	class OpenGlPipeline {
		OpenGlModelShader modelShader;
		OpenGlEdgeShader edgeShader;
		OpenGlGroundShadowShader groundShadowShader;
		OpenGlDepthOnlyShader depthOnlyShader;
		OpenGlSceneVelocityShader sceneVelocityShader;

	public:
		// 내부 장면 패스 계약으로 OpenGL 셰이더 프로그램들을 초기화한다.
		bool Initialize(const BuiltInShaderPasses& builtInPasses,
			const SceneInputShaderPasses& sceneInputPasses);
		// 장면 패스별 vertex attribute 위치를 모델 VAO 생성용 값으로 반환한다.
		OpenGlSceneAttributeLocations ResolveSceneAttributeLocations() const;
		// 모델 표면 프로그램을 바인딩한다.
		void BindModel() const;
		// 엣지 프로그램을 바인딩한다.
		void BindEdge() const;
		// 지면 그림자 프로그램을 바인딩한다.
		void BindGroundShadow() const;
		// 장면 depth 프로그램을 바인딩한다.
		void BindDepthOnly() const;
		// 장면 velocity 프로그램을 바인딩한다.
		void BindSceneVelocity() const;
	};
}
