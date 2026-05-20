#pragma once

#include "../Animation/Model/Animation.h"

#include <memory>

namespace Chrivent {
    struct Model;
    class Drawer;
    class Viewer;

    struct InstanceInfo {
        InstanceInfo();
        virtual ~InstanceInfo();

        std::shared_ptr<Model>	    model;
        std::unique_ptr<Animation>	anim;
        float scale = 1.0f;
    };
    
    class Instance {
    protected:
        std::unique_ptr<InstanceInfo> info;
        std::unique_ptr<Drawer> drawer;
    
	public:
        Instance();
		virtual ~Instance();
        
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;

        InstanceInfo& GetInfo() { return *info; }
        const InstanceInfo& GetInfo() const { return *info; }

        virtual void Clear() {}
        // 렌더러별 모델 리소스를 생성하고 인스턴스를 초기화한다.
        virtual bool Setup(Viewer& baseViewer) = 0;
        // 모델의 동적 버텍스/상태를 렌더러 리소스에 반영한다.
        virtual void Update() const = 0;
        // 현재 인스턴스를 드로어가 가진 패스 순서대로 화면에 그린다.
        void Draw() const;
        // 뷰어 시간과 애니메이션 설정을 기준으로 모델 애니메이션을 갱신한다.
        void UpdateAnimation(const Viewer& viewer) const;
    };
}
