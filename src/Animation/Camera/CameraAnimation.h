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

    struct CameraAnimationInfo {
        std::vector<CameraAnimationKey>	keys;
        Camera camera;
    };

    class CameraAnimation {
        CameraAnimationInfo info;

    public:
        // 카메라 애니메이션 정보를 반환한다.
        const CameraAnimationInfo& GetInfo() const { return info; }
        // 카메라 애니메이션 정보를 통째로 교체한다.
        void SetInfo(CameraAnimationInfo animationInfo) { info = std::move(animationInfo); }
        // 카메라 트랙의 가장 마지막 키 프레임을 반환한다.
        int32_t GetLastFrame() const;
        // 지정한 시간의 카메라 키를 보간해 현재 카메라에 적용한다.
        void Evaluate(float t);
    };
}
