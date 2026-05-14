#include "Model.h"

#include "ModelLoader.h"

namespace Chrivent {
	Model::~Model() {
		Destroy();
	}

	bool Model::Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir) {
		const ModelLoader loader(*this);
		return loader.Load(filepath, dataDir);
	}

	void Model::Destroy() {
		materials.clear();
		subMeshes.clear();
		positions.clear();
		normals.clear();
		uvs.clear();
		vertexBoneInfos.clear();
		indices.clear();
		sortedNodes.clear();
		nodes.clear();
		updateRanges.clear();
		for (const auto& joint : joints)
			physics->world->removeConstraint(joint->constraint.get());
		joints.clear();
		for (const auto& rb : rigidBodies)
			physics->world->removeRigidBody(rb->rigidBody.get());
		rigidBodies.clear();
		physics.reset();
	}
}
