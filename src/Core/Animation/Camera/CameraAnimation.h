#pragma once

#include "Core/Animation/Bezier.h"
#include "Core/Animation/Camera/Camera.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace Chrivent {
    struct CameraAnimationKey {
        uint32_t	frame = 0;
        glm::vec3	interest;
        glm::vec3	rotate;
        float		distance;
        float		fov;
        Bezier		ixBezier;
        Bezier		iyBezier;
        Bezier		izBezier;
        Bezier		rotateBezier;
        Bezier		distanceBezier;
        Bezier		fovBezier;
    };

    class CameraAnimation {
        std::vector<CameraAnimationKey> keys;
        Camera camera;

    public:
        explicit CameraAnimation(std::vector<CameraAnimationKey> animationKeys) : keys(std::move(animationKeys)) {}

        const std::vector<CameraAnimationKey>& GetKeys() const { return keys; }

        uint32_t GetLastFrame() const { return keys.empty() ? 0 : keys.back().frame; }
        // 지정한 시간의 카메라 키를 보간해 현재 카메라를 반환한다.
        const Camera& Evaluate(float t);
    };
}
