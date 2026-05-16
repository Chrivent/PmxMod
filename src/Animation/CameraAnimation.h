#pragma once

#include "Bezier.h"

namespace Chrivent {
    struct Camera {
        glm::vec3	interest = glm::vec3(0, 10, 0);
        glm::vec3	rotate = glm::vec3(0, 0, 0);
        float		distance = 50;
        float		fov = glm::radians(30.0f);

        // 현재 카메라 파라미터로 뷰 행렬을 계산한다.
        glm::mat4 CalcViewMatrix() const;
    };

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
