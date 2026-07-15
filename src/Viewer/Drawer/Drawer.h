#pragma once

#include "Viewer/Shader/ShaderConstants.h"

#include <glm/glm.hpp>

namespace Chrivent {
	struct Material;
	class Viewer;

	// 모델의 기본, 엣지, 그림자와 보조 패스를 그리는 공통 규약을 정의한다.
	class Drawer {
	protected:
		Viewer& viewer;

		// 현재 프레임에서 사용할 렌더러별 임시 리소스를 초기화한다.
		virtual void BeginDrawFrame() {}
		// 렌더러별 clip space 보정 행렬을 반환한다.
		virtual const glm::mat4& ClipMatrix() const;
		// 지면 그림자 투영에 사용할 평면 그림자 행렬을 만든다.
		static glm::mat4 BuildGroundShadowMatrix(const glm::vec3& lightDir);
		// 모델 배율을 적용한 공통 월드 행렬을 만든다.
		static glm::mat4 BuildWorldMatrix(float scale);
		// 모델 패스의 공통 vertex 상수를 만든다.
		static ModelVertexConstants BuildModelVertexConstants(
			const Viewer& viewer, const glm::mat4& world, const glm::mat4& clipMatrix);
		// 모델 패스의 공통 material 및 조명 상수를 만든다.
		static ModelPixelConstants BuildModelPixelConstants(const Viewer& viewer, const Material& material,
			int textureMode, int toonTextureMode, int sphereTextureMode);
		// 엣지 패스의 공통 vertex 상수를 만든다.
		static EdgeVertexConstants BuildEdgeVertexConstants(const Viewer& viewer, const glm::mat4& world,
			const glm::mat4& clipMatrix, const glm::vec2& screenSize);
		// 지면 그림자 패스의 공통 vertex 상수를 만든다.
		static GroundShadowVertexConstants BuildGroundShadowVertexConstants(
			const Viewer& viewer, const glm::mat4& world, const glm::mat4& clipMatrix);
		// 시간 기반 후처리 장면 입력의 현재 및 이전 프레임 행렬을 만든다.
		static SceneVelocityVertexConstants BuildSceneVelocityVertexConstants(
			const Viewer& viewer, const glm::mat4& world, const glm::mat4& clipMatrix);
		// material 불투명도가 공통 후처리 장면 입력 규격을 만족하는지 확인한다.
		static bool ShouldDrawPostProcessSurface(float opacity);
		// 공통 장면 입력에 사용할 material과 texture alpha 판정 값을 만든다.
		static SceneSurfacePixelConstants BuildSceneSurfacePixelConstants(float opacity, bool textureHasAlpha);

		// 일반 메시 패스를 그린다.
		virtual void DrawModel() = 0;
		// 엣지 패스를 그린다.
		virtual void DrawEdge() = 0;
		// 지면 그림자 패스를 그린다.
		virtual void DrawGroundShadow() = 0;
		// 후처리가 요구하는 장면 depth와 velocity 입력에 모델 geometry를 기록한다.
		virtual void DrawSceneInputs() = 0;

	public:
		explicit Drawer(Viewer& sourceViewer) : viewer(sourceViewer) {}
		virtual ~Drawer();

		// 현재 드로어가 가진 패스 순서대로 화면에 그린다.
		void Draw();
		// 후처리 장면 depth와 velocity 입력 패스를 그린다.
		void DrawPostProcessSceneInputs();
	};
}
