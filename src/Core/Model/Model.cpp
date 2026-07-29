#include "Core/Model/Model.h"

namespace Chrivent {
	void ModelSkeletonData::Swap(ModelSkeletonData& other) {
		nodes.swap(other.nodes);
		ikSolvers.swap(other.ikSolvers);
		transforms.swap(other.transforms);
		sortedNodes.swap(other.sortedNodes);
		displayFrames.swap(other.displayFrames);
		++structureRevision;
		++other.structureRevision;
	}

	void ModelMorphData::Swap(ModelMorphData& other) {
		morphs.swap(other.morphs);
		positionMorphs.swap(other.positionMorphs);
		uvMorphs.swap(other.uvMorphs);
		materialMorphs.swap(other.materialMorphs);
		boneMorphs.swap(other.boneMorphs);
		groupMorphs.swap(other.groupMorphs);
		morphPositions.swap(other.morphPositions);
		morphUVs.swap(other.morphUVs);
		++structureRevision;
		++other.structureRevision;
	}

	Model::Model() : skeletonData(structureRevision), morphData(structureRevision) {}
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
		skeletonData.Swap(other.skeletonData);
		morphData.Swap(other.morphData);
		physicsData.Swap(other.physicsData);
	}

	void Model::AccumulateMorphs() {
		morphEvaluator.Update(*this);
	}

	std::expected<void, PhysicsError> Model::InitializePhysics(const std::vector<RigidBodyDefinition>& rigidBodies,
		const std::vector<JointDefinition>& joints) {
		return physicsData.Initialize(rigidBodies, joints, skeletonData.GetNodes());
	}

	void Model::ResetPhysics() const {
		physicsData.ResetSimulation(skeletonData.GetNodes());
	}

	void Model::UpdatePhysics(const float elapsed) const {
		physicsData.UpdateSimulation(elapsed, skeletonData.GetNodes());
	}
}
