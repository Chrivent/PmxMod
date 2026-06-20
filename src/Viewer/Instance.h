#pragma once

#include "../Animation/Model/Animation.h"

#include <memory>

namespace Chrivent {
    struct Model;
    struct ModelUpdateTiming;
    struct ViewerInfo;
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

        // 렌더러별 인스턴스 리소스를 해제한다.
        virtual void Clear() {}
        // 렌더러별 모델 리소스를 생성하고 인스턴스를 초기화한다.
        virtual bool Setup(Viewer& baseViewer) = 0;
        // 모델의 동적 버텍스/상태를 렌더러 리소스에 반영한다.
        virtual void Upload() const = 0;
        // 현재 인스턴스를 드로어가 가진 패스 순서대로 화면에 그린다.
        void Draw() const;
        // 뷰어 시간을 기준으로 애니메이션, 물리, 본 행렬과 스키닝 범위를 준비한다.
        void PrepareUpdate(const ViewerInfo& viewerInfo, ModelUpdateTiming* timing = nullptr) const;
        // 현재 모델의 스키닝 작업 범위 개수를 반환한다.
        std::size_t GetSkinningTaskCount() const;
        // 지정된 범위의 CPU 스키닝을 수행한다.
        void UpdateSkinning(std::size_t taskIndex) const;
    };
}
