#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	// 모델 vertex shader에 전달할 월드-뷰 및 투영 행렬을 보관한다.
	struct ModelVertexConstants {
		glm::mat4 wv;
		glm::mat4 wvp;
	};

	// 장면 속도 계산에 사용할 현재 프레임과 이전 프레임 행렬을 보관한다.
	struct SceneVelocityVertexConstants {
		glm::mat4 currentWvp;
		glm::mat4 previousWvp;
	};

	// 모델 pixel shader에 전달할 재질과 조명 상수를 보관한다.
	struct ModelPixelConstants {
		glm::vec4 texMulFactor{};
		glm::vec4 texAddFactor{};
		glm::vec4 toonTexMulFactor{};
		glm::vec4 toonTexAddFactor{};
		glm::vec4 sphereTexMulFactor{};
		glm::vec4 sphereTexAddFactor{};
		glm::ivec4 textureModes{};
		glm::vec4 diffuseAlpha{};
		glm::vec4 ambientSpecularPower{};
		glm::vec4 specular{};
		glm::vec4 lightColor{};
		glm::vec4 lightDir{};
	};

	// 외곽선 vertex shader에 전달할 변환과 화면 크기 및 굵기를 보관한다.
	struct EdgeVertexConstants {
		glm::mat4 wv;
		glm::mat4 wvp;
		glm::vec2 screenSize{};
		float edgeSize = 0.0f;
		float padding = 0.0f;
	};

	// 외곽선 pixel shader에 전달할 외곽선 색상을 보관한다.
	struct EdgePixelConstants {
		glm::vec4 edgeColor{};
	};

	// 지면 그림자 vertex shader에 전달할 변환 행렬을 보관한다.
	struct GroundShadowVertexConstants {
		glm::mat4 wvp;
	};

	// 지면 그림자 pixel shader에 전달할 그림자 색상을 보관한다.
	struct GroundShadowPixelConstants {
		glm::vec4 shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
	};
}
