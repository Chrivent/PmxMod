#include "Core/Model/Model.h"

namespace Chrivent {
	Model::~Model() {
		Reset();
	}

	void Model::Reset() {
		geometryData.updateRanges.clear();
		geometryData.parallelUpdateCount = 0;
		if (physicsData.physics && physicsData.physics->world) {
			for (const auto& joint : physicsData.joints) {
				if (joint && joint->GetConstraint())
					physicsData.physics->world->removeConstraint(joint->GetConstraint());
			}
			for (const auto& rb : physicsData.rigidBodies) {
				if (rb && rb->rigidBody)
					physicsData.physics->world->removeRigidBody(rb->rigidBody.get());
			}
		}
		physicsData.joints.clear();
		physicsData.rigidBodies.clear();
		physicsData.physics.reset();
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
	}
}
