#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	struct VulkanModelVertexConstants {
		glm::mat4 wv;
		glm::mat4 wvp;
	};

	struct VulkanModelPixelConstants {
		float alpha = 1.0f;
		alignas(16) glm::vec3 diffuse = glm::vec3(1.0f);
		alignas(16) glm::vec3 ambient = glm::vec3(0.2f);
		float specularPower = 1.0f;
		alignas(16) glm::vec3 specular = glm::vec3(0.0f);
		alignas(16) glm::vec3 lightColor = glm::vec3(1.0f);
		alignas(16) glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
		alignas(16) glm::vec4 texMulFactor = glm::vec4(1.0f);
		glm::vec4 texAddFactor = glm::vec4(0.0f);
		glm::vec4 toonTexMulFactor = glm::vec4(1.0f);
		glm::vec4 toonTexAddFactor = glm::vec4(0.0f);
		glm::vec4 sphereTexMulFactor = glm::vec4(1.0f);
		glm::vec4 sphereTexAddFactor = glm::vec4(0.0f);
		glm::ivec4 textureModes = glm::ivec4(0);
	};

	struct VulkanEdgeVertexConstants {
		glm::mat4 wv;
		glm::mat4 wvp;
		glm::vec2 screenSize;
		float edgeSize = 0.0f;
	};

	struct VulkanEdgePixelConstants {
		glm::vec4 edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	};

	struct VulkanGroundShadowVertexConstants {
		glm::mat4 wvp;
	};

	struct VulkanGroundShadowPixelConstants {
		glm::vec4 shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
	};
}
