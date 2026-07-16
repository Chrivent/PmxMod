#pragma once

#include "Core/Animation/Model/Animation.h"

#include <memory>

namespace Chrivent {
    struct ModelUpdateTiming;
    class Model;
    class Drawer;

	// 모델 애니메이션과 물리 갱신에 필요한 프레임 입력만 전달한다.
	struct InstanceUpdateState {
		float animationFrame = 0.0f;
		float elapsed = 0.0f;
		bool velocityRequired = false;
		bool physicsEnabled = false;
	};

    // 한 모델의 API별 GPU 리소스와 그리기 객체를 소유하는 공통 규약을 정의한다.
    class Instance {
        // 렌더러가 신뢰할 모델 geometry와 material 범위 불변식을 검증한다.
        static bool ValidateModel(const Model& sourceModel);

    protected:
        std::unique_ptr<Drawer> drawer;
        std::shared_ptr<Model> model;
        std::unique_ptr<Animation> animation;
        float scale = 1.0f;

        // 렌더러별 모델 GPU 리소스를 초기 상태로 되돌린다.
        virtual void ResetRendererResources() = 0;
        // 렌더러별 모델 리소스를 생성하고 인스턴스를 초기화한다.
        virtual bool SetupRenderer() = 0;

    public:
        Instance();
        virtual ~Instance();
        
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        Model& GetModel() { return *model; }
        const Model& GetModel() const { return *model; }
        Animation* GetAnimation() { return animation.get(); }
        const Animation* GetAnimation() const { return animation.get(); }
        float GetScale() const { return scale; }

        // 모델, 애니메이션과 배율을 확정한 뒤 렌더러별 리소스를 원자적으로 초기화한다.
        bool Initialize(std::shared_ptr<Model> sourceModel, std::unique_ptr<Animation> sourceAnimation,
            float sourceScale);
        // 모델의 동적 버텍스/상태를 렌더러 리소스에 반영한다.
        virtual bool Upload() = 0;
        // 현재 인스턴스를 드로어가 가진 패스 순서대로 화면에 그린다.
        void Draw() const;
        // 현재 인스턴스를 후처리 장면 depth와 velocity 입력 패스에 그린다.
        void DrawPostProcessSceneInputs() const;
		// 프레임 입력을 기준으로 애니메이션, 본 행렬과 스키닝 범위를 준비한다.
		void PrepareUpdate(const InstanceUpdateState& state, ModelUpdateTiming* timing = nullptr) const;
        // 연결된 모델의 정점 갱신 범위를 기준으로 스키닝 작업 수를 계산한다.
        std::size_t CalculateSkinningTaskCount() const;
        // 지정된 범위의 CPU 스키닝을 수행한다.
        void UpdateSkinning(std::size_t taskIndex) const;
    };
}
