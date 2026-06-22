#pragma once

#include "../ShaderConstants.h"

namespace Chrivent {
	struct GlslModelPixelConstants : ShaderModelPixelConstants {
		glm::vec4	diffuseAlpha;
		glm::vec4	ambientSpecularPower;
		glm::vec4	specular;
		glm::vec4	lightColor;
		glm::vec4	lightDir;
	};

	struct GlslEdgeVertexConstants : ShaderEdgeVertexConstants {
		float		edgeSize;
		float		padding;
	};
}
