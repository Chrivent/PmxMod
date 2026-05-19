#pragma once

#include "../Bezier.h"
#include "../../Program/Manager/CameraManager.h"

namespace Chrivent {
    struct CameraAnimationKey {
        int32_t		time;
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

    struct CameraAnimation {
        std::vector<CameraAnimationKey>	keys;
        Camera camera;
        
        // 지정한 시간의 카메라 키를 보간해 현재 카메라에 적용한다.
        void Evaluate(float t);
    };
}
