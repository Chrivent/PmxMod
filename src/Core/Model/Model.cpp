#include "Core/Model/Model.h"

namespace Chrivent {
	Model::Model() = default;
	Model::~Model() = default;

	bool Model::HasPhysics() const {
		return physicsData.IsInitialized();
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
		physicsData.Swap(other.physicsData);
		structureRevision++;
		other.structureRevision++;
	}

	void Model::AccumulateMorphs() {
		morphEvaluator.Update(*this);
	}

	std::expected<void, PhysicsError> Model::InitializePhysics(const std::vector<RigidBodyDefinition>& rigidBodies,
		const std::vector<JointDefinition>& joints) {
		return physicsData.Initialize(rigidBodies, joints, skeletonData.GetNodes());
	}

	void Model::ResetPhysics() {
		physicsData.ResetSimulation(skeletonData.GetNodes());
	}

	void Model::UpdatePhysics(const float elapsed) {
		physicsData.UpdateSimulation(elapsed, skeletonData.GetNodes());
	}
}
