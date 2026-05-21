#pragma once

#include "../Bezier.h"
#include "../../Program/Manager/CameraManager.h"

#include <cstdint>

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

    struct CameraAnimationInfo {
        std::vector<CameraAnimationKey>	keys;
        Camera camera;
    };

    class CameraAnimation {
        CameraAnimationInfo info;

    public:
        const CameraAnimationInfo& GetInfo() const { return info; }
        void SetInfo(CameraAnimationInfo animationInfo) { info = std::move(animationInfo); }
        
        // 카메라 트랙의 가장 마지막 키 프레임을 반환한다.
        uint32_t GetLastFrame() const;
        // 지정한 시간의 카메라 키를 보간해 현재 카메라에 적용한다.
        void Evaluate(float t);
    };
}
