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
        
        // 吏?뺥븳 ?쒓컙??移대찓???ㅻ? 蹂닿컙???꾩옱 移대찓?쇱뿉 ?곸슜?쒕떎.
        void Evaluate(float t);
    };
}
