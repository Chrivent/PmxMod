#include "MMDModel.h"
#include "MMDPhysics.h"
#include "VMDAnimation.h"

#include <glm/gtc/matrix_transform.hpp>

namespace saba
{
	size_t MMDNodeManager::FindNodeIndex(const std::string& name)
	{
		auto findIt = std::find_if(
			m_nodes.begin(),
			m_nodes.end(),
			[&name](const std::unique_ptr<MMDNode>& node) { return node->m_name == name; }
		);
		if (findIt == m_nodes.end())
		{
			return NPos;
		}
		else
		{
			return findIt - m_nodes.begin();
		}
	}

	MMDNode* MMDNodeManager::GetNode(size_t idx)
	{
		return m_nodes[idx].get();
	}

	MMDNode* MMDNodeManager::GetNode(const std::string& nodeName)
	{
		auto findIdx = FindNodeIndex(nodeName);
		if (findIdx == NPos)
		{
			return nullptr;
		}
		return GetNode(findIdx);
	}

	MMDNode* MMDNodeManager::AddNode()
	{
		auto node = std::make_unique<MMDNode>();
		node->m_index = (uint32_t)m_nodes.size();
		m_nodes.emplace_back(std::move(node));
		return m_nodes[m_nodes.size() - 1].get();
	}

	size_t MMDIKManager::FindIKSolverIndex(const std::string& name)
	{
		auto findIt = std::find_if(
			m_ikSolvers.begin(),
			m_ikSolvers.end(),
			[&name](const std::unique_ptr<MMDIkSolver>& ikSolver) { return ikSolver->GetName() == name; }
		);
		if (findIt == m_ikSolvers.end())
		{
			return NPos;
		}
		else
		{
			return findIt - m_ikSolvers.begin();
		}
	}

	MMDIkSolver* MMDIKManager::GetIKSolver(size_t idx)
	{
		return m_ikSolvers[idx].get();
	}

	MMDIkSolver* MMDIKManager::GetIKSolver(const std::string& ikName)
	{
		auto findIdx = FindIKSolverIndex(ikName);
		if (findIdx == NPos)
		{
			return nullptr;
		}
		return GetIKSolver(findIdx);
	}

	MMDIkSolver* MMDIKManager::AddIKSolver()
	{
		m_ikSolvers.emplace_back(std::make_unique<MMDIkSolver>());
		return m_ikSolvers[m_ikSolvers.size() - 1].get();
	}

	size_t MMDMorphManager::FindMorphIndex(const std::string& name)
	{
		auto findIt = std::find_if(
			m_morphs.begin(),
			m_morphs.end(),
			[&name](const std::unique_ptr<MMDMorph>& morph) { return morph->m_name == name; }
		);
		if (findIt == m_morphs.end())
		{
			return NPos;
		}
		else
		{
			return findIt - m_morphs.begin();
		}
	}

	MMDMorph* MMDMorphManager::GetMorph(size_t idx)
	{
		return m_morphs[idx].get();
	}

	MMDMorph* MMDMorphManager::GetMorph(const std::string& name)
	{
		auto findIdx = FindMorphIndex(name);
		if (findIdx == NPos)
		{
			return nullptr;
		}
		return GetMorph(findIdx);
	}

	MMDMorph* MMDMorphManager::AddMorph()
	{
		m_morphs.emplace_back(std::make_unique<MMDMorph>());
		return m_morphs[m_morphs.size() - 1].get();
	}

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

	MMDModel::MMDModel()
		: m_parallelUpdateCount(0)
	{
	}

	void MMDModel::SaveBaseAnimation()
	{
		auto nodeMan = GetNodeManager();
		for (size_t i = 0; i < nodeMan->m_nodes.size(); i++)
		{
			auto node = nodeMan->GetNode(i);
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
			auto ikSolver = ikMan->GetIKSolver(i);
			ikSolver->SaveBaseAnimation();
		}
	}

	void MMDModel::LoadBaseAnimation()
	{
		auto nodeMan = GetNodeManager();
		for (size_t i = 0; i < nodeMan->m_nodes.size(); i++)
		{
			auto node = nodeMan->GetNode(i);
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
			auto ikSolver = ikMan->GetIKSolver(i);
			ikSolver->LoadBaseAnimation();
		}
	}

	void MMDModel::ClearBaseAnimation()
	{
		auto nodeMan = GetNodeManager();
		for (size_t i = 0; i < nodeMan->m_nodes.size(); i++)
		{
			auto node = nodeMan->GetNode(i);
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
			auto ikSolver = ikMan->GetIKSolver(i);
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
