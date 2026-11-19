#pragma once

#include <memory>

namespace Chrivent {
	class Physics;

	// 씬의 유일한 물리 시계다. 모델은 강체만 등록하며 월드를 개별적으로 진행하지 않는다.
	class ScenePhysics {
		friend class ModelPhysicsData;
		std::shared_ptr<Physics> world;

		// 강체가 있는 첫 모델을 로드할 때만 Bullet 월드를 생성한다.
		std::shared_ptr<Physics> AcquireWorld();

	public:
		ScenePhysics() = default;
		ScenePhysics(const ScenePhysics&) = delete;
		ScenePhysics& operator=(const ScenePhysics&) = delete;
		ScenePhysics(ScenePhysics&&) = default;
		ScenePhysics& operator=(ScenePhysics&&) = default;

		// 모든 모델의 포즈가 준비된 뒤 진행하고 실제로 계산한 고정 스텝 수를 반환한다.
		int Step(float elapsed) const;
		// 씬 전체의 강체를 현재 포즈로 옮기고 이전 프레임의 잔여 시간을 버린다.
		void Reset() const;
	};
}
