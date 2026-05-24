#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
    struct Dx11ModelVertexConstants {
        glm::mat4   wv;
        glm::mat4   wvp;
    };

    struct Dx11ModelPixelConstants {
        float       alpha;
        glm::vec3   diffuse;
        glm::vec3   ambient;
        float       dummy1;
        glm::vec3   specular;
        float       specularPower;
        glm::vec3   lightColor;
        float       dummy2;
        glm::vec3   lightDir;
        float       dummy3;
        glm::vec4   texMulFactor;
        glm::vec4   texAddFactor;
        glm::vec4   toonTexMulFactor;
        glm::vec4   toonTexAddFactor;
        glm::vec4   sphereTexMulFactor;
        glm::vec4   sphereTexAddFactor;
        glm::ivec4  textureModes;
    };

    struct Dx11EdgeVertexConstants {
        glm::mat4   wv;
        glm::mat4   wvp;
        glm::vec2   screenSize;
        float       dummy[2];
    };

    struct Dx11EdgeSizeConstants {
        float       edgeSize;
        float       dummy[3];
    };

    struct Dx11EdgePixelConstants {
        glm::vec4   edgeColor;
    };

    struct Dx11GroundShadowVertexConstants {
        glm::mat4   wvp;
    };

    struct Dx11GroundShadowPixelConstants {
        glm::vec4   shadowColor;
    };
}
