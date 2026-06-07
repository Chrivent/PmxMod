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
}
