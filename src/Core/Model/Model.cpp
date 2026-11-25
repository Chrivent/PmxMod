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
	Model::~Model() {
		// Bullet 모션 상태가 참조하는 본보다 먼저 월드 등록을 해제한다.
		physicsData.Reset();
	}

	bool Model::HasPhysics() const {
		return physicsData.IsInitialized();
	}

	void Model::Reset() {
		Model emptyModel;
		Swap(emptyModel);
	}

	void Model::Swap(Model& other) {
		std::swap(visible, other.visible);
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

	std::expected<void, PhysicsError> Model::InitializePhysics(ScenePhysics& scene, const std::vector<RigidBodyDefinition>& rigidBodies,
		const std::vector<JointDefinition>& joints, const float scale) {
		return physicsData.Initialize(scene, rigidBodies, joints, skeletonData.GetNodes(), scale);
	}

	void Model::ReflectPhysics() const {
		physicsData.ReflectTransforms();
	}
}
