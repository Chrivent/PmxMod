#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	struct Dx12ModelVertexConstants {
		glm::mat4 wv;
		glm::mat4 wvp;
	};

	struct Dx12ModelPixelConstants {
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
		glm::vec4 texMulFactor{};
		glm::vec4 texAddFactor{};
		glm::vec4 toonTexMulFactor{};
		glm::vec4 toonTexAddFactor{};
		glm::vec4 sphereTexMulFactor{};
		glm::vec4 sphereTexAddFactor{};
		glm::ivec4 textureModes{};
	};

	struct Dx12EdgeVertexConstants {
		glm::mat4 wv;
		glm::mat4 wvp;
		glm::vec2 screenSize{};
		float padding[2]{};
	};

	struct Dx12EdgeSizeConstants {
		float edgeSize = 0.0f;
		float padding[3]{};
	};

	struct Dx12EdgePixelConstants {
		glm::vec4 edgeColor{};
	};

	struct Dx12GroundShadowVertexConstants {
		glm::mat4 wvp;
	};

	struct Dx12GroundShadowPixelConstants {
		glm::vec4 shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
	};
}
