#include "Core/Model/Model.h"

#include "Core/Model/Physics/Physics.h"

namespace Chrivent {
	Model::Model() : physicsData(std::make_unique<ModelPhysicsData>()) {}
	Model::~Model() = default;

	bool Model::HasPhysics() const {
		return physicsData && physicsData->physics;
	}

	void Model::Reset() {
		geometryData.updateRanges.clear();
		physicsData->Reset();
		infoData.modelName.clear();
		infoData.englishModelName.clear();
		infoData.comment.clear();
		infoData.englishComment.clear();
		materialData.materials.clear();
		materialData.initMaterials.clear();
		materialData.mulMaterialFactors.clear();
		materialData.addMaterialFactors.clear();
		materialData.subMeshes.clear();
		geometryData.positions.clear();
		geometryData.normals.clear();
		geometryData.uvs.clear();
		geometryData.vertexBoneInfos.clear();
		geometryData.indices.clear();
		geometryData.indexCount = 0;
		geometryData.indexElementSize = 0;
		skeletonData.sortedNodes.clear();
		skeletonData.nodes.clear();
		skeletonData.ikSolvers.clear();
		skeletonData.transforms.clear();
		skeletonData.displayFrames.clear();
		morphData.morphs.clear();
		morphData.positionMorphs.clear();
		morphData.uvMorphs.clear();
		morphData.materialMorphs.clear();
		morphData.boneMorphs.clear();
		morphData.groupMorphs.clear();
		morphData.morphPositions.clear();
		morphData.morphUVs.clear();
		geometryData.updatePositions.clear();
		geometryData.updateNormals.clear();
		geometryData.updateUVs.clear();
		geometryData.previousPositions.clear();
		geometryData.bboxMin = glm::vec3(0);
		geometryData.bboxMax = glm::vec3(0);
	}

	void Model::Swap(Model& other) {
		std::swap(infoData, other.infoData);
		std::swap(geometryData, other.geometryData);
		std::swap(materialData, other.materialData);
		std::swap(skeletonData, other.skeletonData);
		std::swap(morphData, other.morphData);
		std::swap(physicsData, other.physicsData);
	}
}
