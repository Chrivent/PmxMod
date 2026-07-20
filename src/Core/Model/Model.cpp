#include "Core/Model/Model.h"

#include "Core/Model/Physics/Physics.h"

namespace Chrivent {
	Model::Model() : physicsData(std::make_unique<ModelPhysicsData>()) {}
	Model::~Model() = default;

	bool Model::HasPhysics() const {
		return physicsData->IsInitialized();
	}

	void Model::Reset() {
		Model emptyModel;
		Swap(emptyModel);
	}

	void Model::Swap(Model& other) {
		std::swap(infoData, other.infoData);
		std::swap(geometryData, other.geometryData);
		std::swap(materialData, other.materialData);
		std::swap(skeletonData, other.skeletonData);
		std::swap(morphData, other.morphData);
		std::swap(physicsData, other.physicsData);
	}

	void Model::InitializePhysics(const std::vector<RigidBodyDefinition>& rigidBodies,
		const std::vector<JointDefinition>& joints) const {
		physicsData->Initialize(rigidBodies, joints, skeletonData.nodes);
	}

	void Model::ResetPhysics() const {
		physicsData->ResetSimulation(skeletonData.nodes);
	}

	void Model::UpdatePhysics(const float elapsed) const {
		physicsData->UpdateSimulation(elapsed, skeletonData.nodes);
	}
}
