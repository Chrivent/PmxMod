#include "Core/Model/Physics/ScenePhysics.h"

#include "Core/Model/Physics/Physics.h"

namespace Chrivent {
	std::shared_ptr<Physics> ScenePhysics::AcquireWorld() {
		if (!world)
			world = std::make_shared<Physics>();
		return world;
	}

	int ScenePhysics::Step(const float elapsed) const {
		return world ? world->StepSimulation(elapsed) : 0;
	}

	void ScenePhysics::Reset() const {
		if (world)
			world->ResetSimulation();
	}
}
