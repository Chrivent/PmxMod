#pragma once

#include "../ShaderConstants.h"

namespace Chrivent {
	struct HlslModelPixelConstants : ShaderModelPixelConstants {
		float alpha = 0.0f;
		glm::vec3 diffuse{};
		glm::vec3 ambient{};
		float dummy1 = 0.0f;
		glm::vec3 specular{};
		float specularPower = 0.0f;
		glm::vec3 lightColor{};
		float dummy2 = 0.0f;
		glm::vec3 lightDir{};
		float dummy3 = 0.0f;
	};

	struct HlslEdgeVertexConstants : ShaderEdgeVertexConstants {
		float padding[2]{};
	};

	struct HlslEdgeSizeConstants {
		float edgeSize = 0.0f;
		float padding[3]{};
	};
}
