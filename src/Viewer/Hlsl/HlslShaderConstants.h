#pragma once

#include "../ShaderConstants.h"

namespace Chrivent {
	struct HlslModelPixelConstants : ShaderModelPixelConstants {
		glm::vec4 diffuseAlpha{};
		glm::vec4 ambientSpecularPower{};
		glm::vec4 specular{};
		glm::vec4 lightColor{};
		glm::vec4 lightDir{};
	};

	struct HlslEdgeVertexConstants : ShaderEdgeVertexConstants {
		float padding[2]{};
	};

	struct HlslEdgeSizeConstants {
		float edgeSize = 0.0f;
		float padding[3]{};
	};
}
