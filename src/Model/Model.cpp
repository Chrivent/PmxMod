#include "Model.h"

namespace Chrivent {
	Model::~Model() {
		Destroy();
	}

	void Model::Destroy() {
		for (auto& future : geometry.parallelUpdateFutures) {
			if (future.valid())
				future.wait();
		}
		geometry.parallelUpdateFutures.clear();
		geometry.updateRanges.clear();
		geometry.parallelUpdateCount = 0;
		if (physicsData.physics && physicsData.physics->world) {
			for (const auto& joint : physicsData.joints) {
				if (joint && joint->constraint)
					physicsData.physics->world->removeConstraint(joint->constraint.get());
			}
			for (const auto& rb : physicsData.rigidBodies) {
				if (rb && rb->rigidBody)
					physicsData.physics->world->removeRigidBody(rb->rigidBody.get());
			}
		}
		physicsData.joints.clear();
		physicsData.rigidBodies.clear();
		physicsData.physics.reset();
		info.modelName.clear();
		info.englishModelName.clear();
		info.comment.clear();
		info.englishComment.clear();
		materialData.materials.clear();
		materialData.initMaterials.clear();
		materialData.mulMaterialFactors.clear();
		materialData.addMaterialFactors.clear();
		materialData.subMeshes.clear();
		geometry.positions.clear();
		geometry.normals.clear();
		geometry.uvs.clear();
		geometry.vertexBoneInfos.clear();
		geometry.indices.clear();
		geometry.indexCount = 0;
		geometry.indexElementSize = 0;
		skeleton.sortedNodes.clear();
		skeleton.nodes.clear();
		skeleton.ikSolvers.clear();
		skeleton.transforms.clear();
		morphData.morphs.clear();
		morphData.positionMorphs.clear();
		morphData.uvMorphs.clear();
		morphData.materialMorphs.clear();
		morphData.boneMorphs.clear();
		morphData.groupMorphs.clear();
		morphData.morphPositions.clear();
		morphData.morphUVs.clear();
		geometry.updatePositions.clear();
		geometry.updateNormals.clear();
		geometry.updateUVs.clear();
	}
}
