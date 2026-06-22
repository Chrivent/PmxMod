#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	struct ShaderModelVertexConstants {
		glm::mat4 wv;
		glm::mat4 wvp;
	};

	struct ShaderModelPixelConstants {
		glm::vec4 texMulFactor{};
		glm::vec4 texAddFactor{};
		glm::vec4 toonTexMulFactor{};
		glm::vec4 toonTexAddFactor{};
		glm::vec4 sphereTexMulFactor{};
		glm::vec4 sphereTexAddFactor{};
		glm::ivec4 textureModes{};
	};

	struct ShaderEdgeVertexConstants {
		glm::mat4 wv;
		glm::mat4 wvp;
		glm::vec2 screenSize{};
	};

	struct ShaderEdgePixelConstants {
		glm::vec4 edgeColor{};
	};

	struct ShaderGroundShadowVertexConstants {
		glm::mat4 wvp;
	};

	struct ShaderGroundShadowPixelConstants {
		glm::vec4 shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
	};
}
