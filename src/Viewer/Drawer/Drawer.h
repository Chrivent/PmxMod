#pragma once

#include "Viewer/Shader/ShaderConstants.h"
#include "Viewer/Drawer/SceneDrawState.h"
#include "Viewer/Error/GraphicsError.h"

#include <glm/glm.hpp>

namespace Chrivent {
	struct Material;

	// 모델의 기본, 엣지, 그림자와 보조 패스를 그리는 공통 규약을 정의한다.
	class Drawer {
		GraphicsApi graphicsApi = GraphicsApi::Unknown;

	protected:
		// 모델 셰이더에 전달할 기본, 툰, 스피어 텍스처 사용 방식을 보관한다.
		struct MaterialTextureModes {
			int base = 0;
			int toon = 0;
			int sphere = 0;
		};

		SceneDrawState drawState;

		// 현재 API와 작업 문맥을 포함한 구조화된 그래픽 오류를 생성한다.
		GraphicsError CreateGraphicsError(GraphicsErrorCode code, std::string operation,
			std::string message, int64_t nativeCode = 0, bool hasNativeCode = false) const;
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
			const SceneDrawState& state, const glm::mat4& world, const glm::mat4& clipMatrix);
		// 모델 패스의 공통 material 및 조명 상수를 만든다.
		static ModelPixelConstants BuildModelPixelConstants(const SceneDrawState& state, const Material& material,
			int textureMode, int toonTextureMode, int sphereTextureMode);
		// 재질과 실제 GPU 텍스처 보유 여부로 모델 셰이더의 텍스처 모드를 결정한다.
		static MaterialTextureModes ResolveMaterialTextureModes(const Material& material,
			bool baseAvailable, bool baseHasAlpha, bool toonAvailable, bool sphereAvailable);
		// 모델 본체 패스에서 재질을 그려야 하는지 반환한다.
		static bool ShouldDrawModelMaterial(const Material& material);
		// 엣지 패스에서 재질을 그려야 하는지 반환한다.
		static bool ShouldDrawEdgeMaterial(const Material& material);
		// 지면 그림자 패스에서 재질을 그려야 하는지 반환한다.
		static bool ShouldDrawGroundShadowMaterial(const Material& material);
		// 엣지 패스의 공통 vertex 상수를 만든다.
		static EdgeVertexConstants BuildEdgeVertexConstants(const SceneDrawState& state, const glm::mat4& world,
			const glm::mat4& clipMatrix, const glm::vec2& screenSize);
		// 지면 그림자 패스의 공통 vertex 상수를 만든다.
		static GroundShadowVertexConstants BuildGroundShadowVertexConstants(
			const SceneDrawState& state, const glm::mat4& world, const glm::mat4& clipMatrix);
		// 시간 기반 후처리 장면 입력의 현재 및 이전 프레임 행렬을 만든다.
		static SceneVelocityVertexConstants BuildSceneVelocityVertexConstants(
			const SceneDrawState& state, const glm::mat4& world, const glm::mat4& clipMatrix);
		// material 불투명도가 공통 후처리 장면 입력 규격을 만족하는지 확인한다.
		static bool ShouldDrawPostProcessSurface(float opacity);
		// 공통 장면 입력에 사용할 material과 texture alpha 판정 값을 만든다.
		static SceneSurfacePixelConstants BuildSceneSurfacePixelConstants(float opacity, bool textureHasAlpha);

		// 일반 메시 패스를 그린다.
		virtual GraphicsError::Result<void> DrawModel() = 0;
		// 엣지 패스를 그린다.
		virtual GraphicsError::Result<void> DrawEdge() = 0;
		// 지면 그림자 패스를 그린다.
		virtual GraphicsError::Result<void> DrawGroundShadow() = 0;
		// 후처리가 요구하는 장면 depth와 velocity 입력에 모델 geometry를 기록한다.
		virtual GraphicsError::Result<void> DrawSceneInputs() = 0;

	public:
		explicit Drawer(GraphicsApi sourceGraphicsApi);
		virtual ~Drawer();

		// 현재 프레임에서 사용할 렌더러별 임시 리소스를 준비한다.
		void BeginDraw(const SceneDrawState& state);
		// 현재 인스턴스의 모델 본체 패스를 그린다.
		GraphicsError::Result<void> DrawModelPass();
		// 현재 인스턴스의 엣지 패스를 그린다.
		GraphicsError::Result<void> DrawEdgePass();
		// 현재 인스턴스의 지면 그림자 패스를 그린다.
		GraphicsError::Result<void> DrawGroundShadowPass();
		// 후처리 장면 depth와 velocity 입력 패스를 그린다.
		GraphicsError::Result<void> DrawPostProcessSceneInputs();
	};
}
