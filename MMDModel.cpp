#include "MMDModel.h"
#include "MMDPhysics.h"
#include "VMDAnimation.h"

#include <glm/gtc/matrix_transform.hpp>

namespace saba
{
	MMDPhysicsManager::MMDPhysicsManager()
	{
	}

	MMDPhysicsManager::~MMDPhysicsManager()
	{
		for (auto& joint : m_joints)
		{
			m_mmdPhysics->RemoveJoint(joint.get());
		}
		m_joints.clear();

		for (auto& rb : m_rigidBodys)
		{
			m_mmdPhysics->RemoveRigidBody(rb.get());
		}
		m_rigidBodys.clear();

		m_mmdPhysics.reset();
	}

	bool MMDPhysicsManager::Create()
	{
		m_mmdPhysics = std::make_unique<MMDPhysics>();
		return m_mmdPhysics->Create();
	}

	MMDPhysics* MMDPhysicsManager::GetMMDPhysics()
	{
		return m_mmdPhysics.get();
	}

	MMDRigidBody* MMDPhysicsManager::AddRigidBody()
	{
		auto rigidBody = std::make_unique<MMDRigidBody>();
		auto ret = rigidBody.get();
		m_rigidBodys.emplace_back(std::move(rigidBody));

		return ret;
	}

	MMDJoint* MMDPhysicsManager::AddJoint()
	{
		auto joint = std::make_unique<MMDJoint>();
		auto ret = joint.get();
		m_joints.emplace_back(std::move(joint));

		return ret;
	}

	void MMDModel::SaveBaseAnimation()
	{
		auto nodeMan = GetNodeManager();
		for (size_t i = 0; i < nodeMan->m_nodes.size(); i++)
		{
			auto node = nodeMan->GetMMDNode(i);
			node->SaveBaseAnimation();
		}

		auto morphMan = GetMorphManager();
		for (size_t i = 0; i < morphMan->m_morphs.size(); i++)
		{
			auto morph = morphMan->GetMorph(i);
			morph->SaveBaseAnimation();
		}

		auto ikMan = GetIKManager();
		for (size_t i = 0; i < ikMan->m_ikSolvers.size(); i++)
		{
			auto ikSolver = ikMan->GetMMDIKSolver(i);
			ikSolver->SaveBaseAnimation();
		}
	}

	void MMDModel::LoadBaseAnimation()
	{
		auto nodeMan = GetNodeManager();
		for (size_t i = 0; i < nodeMan->m_nodes.size(); i++)
		{
			auto node = nodeMan->GetMMDNode(i);
			node->LoadBaseAnimation();
		}

		auto morphMan = GetMorphManager();
		for (size_t i = 0; i < morphMan->m_morphs.size(); i++)
		{
			auto morph = morphMan->GetMorph(i);
			morph->LoadBaseAnimation();
		}

		auto ikMan = GetIKManager();
		for (size_t i = 0; i < ikMan->m_ikSolvers.size(); i++)
		{
			auto ikSolver = ikMan->GetMMDIKSolver(i);
			ikSolver->LoadBaseAnimation();
		}
	}

	void MMDModel::ClearBaseAnimation()
	{
		auto nodeMan = GetNodeManager();
		for (size_t i = 0; i < nodeMan->m_nodes.size(); i++)
		{
			auto node = nodeMan->GetMMDNode(i);
			node->ClearBaseAnimation();
		}

		auto morphMan = GetMorphManager();
		for (size_t i = 0; i < morphMan->m_morphs.size(); i++)
		{
			auto morph = morphMan->GetMorph(i);
			morph->ClearBaseAnimation();
		}

		auto ikMan = GetIKManager();
		for (size_t i = 0; i < ikMan->m_ikSolvers.size(); i++)
		{
			auto ikSolver = ikMan->GetMMDIKSolver(i);
			ikSolver->ClearBaseAnimation();
		}
	}

	namespace
	{
		glm::mat3 InvZ(const glm::mat3& m)
		{
			const glm::mat3 invZ = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, -1));
			return invZ * m * invZ;
		}
		glm::quat InvZ(const glm::quat& q)
		{
			auto rot0 = glm::mat3_cast(q);
			auto rot1 = InvZ(rot0);
			return glm::quat_cast(rot1);
		}
	}

	void MMDModel::UpdateAllAnimation(VMDAnimation * vmdAnim, float vmdFrame, float physicsElapsed)
	{
		if (vmdAnim != nullptr)
		{
			vmdAnim->Evaluate(vmdFrame);
		}

		UpdateMorphAnimation();

		UpdateNodeAnimation(false);

		UpdatePhysicsAnimation(physicsElapsed);

		UpdateNodeAnimation(true);
	}

	void MMDModel::UpdateAnimation()
	{
		UpdateMorphAnimation();
		UpdateNodeAnimation(false);
	}

	void MMDModel::UpdatePhysics(float elapsed)
	{
		UpdatePhysicsAnimation(elapsed);
	}
}
