#pragma once

#include "../Model.h"
#include "../Animation/Animation.h"

namespace Chrivent {
    class Viewer;
    
    class Instance {
    protected:
        // 일반 메시 패스를 그린다.
        virtual void DrawModel() const = 0;
        // 엣지 패스를 그린다.
        virtual void DrawEdge() const = 0;
        // 지면 그림자 패스를 그린다.
        virtual void DrawGroundShadow() const = 0;
    
    public:
        virtual ~Instance();

        std::shared_ptr<Model>	    model;
        std::unique_ptr<Animation>	anim;
        float scale = 1.0f;

        // 렌더러별 모델 리소스를 생성하고 인스턴스를 초기화한다.
        virtual bool Setup(Viewer& baseViewer) = 0;
        // 모델의 동적 버텍스/상태를 렌더러 리소스에 반영한다.
        virtual void Update() const = 0;
        // 현재 인스턴스를 패스 순서대로 화면에 그린다.
        void Draw() const;
        // 렌더러별 GPU 리소스를 해제한다.
        virtual void Clear() {}
        // 뷰어 시간과 애니메이션 설정을 기준으로 모델 애니메이션을 갱신한다.
        void UpdateAnimation(const Viewer& viewer) const;
    };
}
