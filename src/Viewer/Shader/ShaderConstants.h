#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	struct ModelVertexConstants {
		glm::mat4 wv;
		glm::mat4 wvp;
	};

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

	struct EdgeVertexConstants {
		glm::mat4 wv;
		glm::mat4 wvp;
		glm::vec2 screenSize{};
		float edgeSize = 0.0f;
		float padding = 0.0f;
	};

	struct EdgePixelConstants {
		glm::vec4 edgeColor{};
	};

	struct GroundShadowVertexConstants {
		glm::mat4 wvp;
	};

	struct GroundShadowPixelConstants {
		glm::vec4 shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
	};
}
