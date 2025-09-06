#pragma once

#include "MMDNode.h"
#include "MMDIkSolver.h"
#include "MMDMorph.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace saba
{
	struct MMDMaterial;
	class MMDPhysics;
	class MMDRigidBody;
	class MMDJoint;
	struct VPDFile;

	class MMDNodeManager
	{
	public:
		static const size_t NPos = -1;

		size_t FindNodeIndex(const std::string& name)
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
		MMDNode* GetNode(size_t idx)
		{
			return m_nodes[idx].get();
		}

		MMDNode* GetNode(const std::string& nodeName)
		{
			auto findIdx = FindNodeIndex(nodeName);
			if (findIdx == NPos)
			{
				return nullptr;
			}
			return GetNode(findIdx);
		}

		MMDNode* AddNode()
		{
			auto node = std::make_unique<MMDNode>();
			node->m_index = (uint32_t)m_nodes.size();
			m_nodes.emplace_back(std::move(node));
			return m_nodes[m_nodes.size() - 1].get();
		}

		std::vector<std::unique_ptr<MMDNode>>* GetNodes()
		{
			return &m_nodes;
		}

		std::vector<std::unique_ptr<MMDNode>>	m_nodes;
	};

	class MMDIKManager
	{
	public:
		static const size_t NPos = -1;

		size_t FindIKSolverIndex(const std::string& name)
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
		MMDIkSolver* GetIKSolver(size_t idx)
		{
			return m_ikSolvers[idx].get();
		}

		MMDIkSolver* GetIKSolver(const std::string& ikName)
		{
			auto findIdx = FindIKSolverIndex(ikName);
			if (findIdx == NPos)
			{
				return nullptr;
			}
			return GetIKSolver(findIdx);
		}

		MMDIkSolver* AddIKSolver()
		{
			m_ikSolvers.emplace_back(std::make_unique<MMDIkSolver>());
			return m_ikSolvers[m_ikSolvers.size() - 1].get();
		}

		std::vector<std::unique_ptr<MMDIkSolver>>* GetIKSolvers()
		{
			return &m_ikSolvers;
		}

		std::vector<std::unique_ptr<MMDIkSolver>>	m_ikSolvers;
	};

	class MMDMorphManager
	{
	public:
		static const size_t NPos = -1;

		size_t FindMorphIndex(const std::string& name)
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
		MMDMorph* GetMorph(size_t idx)
		{
			return m_morphs[idx].get();
		}

		MMDMorph* GetMorph(const std::string& name)
		{
			auto findIdx = FindMorphIndex(name);
			if (findIdx == NPos)
			{
				return nullptr;
			}
			return GetMorph(findIdx);
		}

		MMDMorph* AddMorph()
		{
			m_morphs.emplace_back(std::make_unique<MMDMorph>());
			return m_morphs[m_morphs.size() - 1].get();
		}

		std::vector<std::unique_ptr<MMDMorph>>* GetMorphs()
		{
			return &m_morphs;
		}

		std::vector<std::unique_ptr<MMDMorph>>	m_morphs;
	};

	class MMDPhysicsManager
	{
	public:
		MMDPhysicsManager();
		~MMDPhysicsManager();

		bool Create();

		MMDPhysics* GetMMDPhysics();

		MMDRigidBody* AddRigidBody();
		std::vector<std::unique_ptr<MMDRigidBody>>* GetRigidBodys() { return &m_rigidBodys; }

		MMDJoint* AddJoint();
		std::vector<std::unique_ptr<MMDJoint>>* GetJoints() { return &m_joints; }

		std::unique_ptr<MMDPhysics>	m_mmdPhysics;

		std::vector<std::unique_ptr<MMDRigidBody>>	m_rigidBodys;
		std::vector<std::unique_ptr<MMDJoint>>		m_joints;
	};

	struct MMDSubMesh
	{
		int	m_beginIndex;
		int	m_vertexCount;
		int	m_materialID;
	};

	class VMDAnimation;

	class MMDModel
	{
	public:
		virtual MMDNodeManager* GetNodeManager() = 0;
		virtual MMDIKManager* GetIKManager() = 0;
		virtual MMDMorphManager* GetMorphManager() = 0;
		virtual MMDPhysicsManager* GetPhysicsManager() = 0;

		virtual size_t GetVertexCount() const = 0;
		virtual const glm::vec3* GetPositions() const = 0;
		virtual const glm::vec3* GetNormals() const = 0;
		virtual const glm::vec2* GetUVs() const = 0;
		virtual const glm::vec3* GetUpdatePositions() const = 0;
		virtual const glm::vec3* GetUpdateNormals() const = 0;
		virtual const glm::vec2* GetUpdateUVs() const = 0;

		virtual size_t GetIndexElementSize() const = 0;
		virtual size_t GetIndexCount() const = 0;
		virtual const void* GetIndices() const = 0;

		virtual size_t GetMaterialCount() const = 0;
		virtual const MMDMaterial* GetMaterials() const = 0;

		virtual size_t GetSubMeshCount() const = 0;
		virtual const MMDSubMesh* GetSubMeshes() const = 0;

		virtual MMDPhysics* GetMMDPhysics() = 0;

		// ノードを初期化する
		virtual void InitializeAnimation() = 0;

		// ベースアニメーション(アニメーション読み込み時、Physics反映用)
		void SaveBaseAnimation();
		void LoadBaseAnimation();
		void ClearBaseAnimation();

		// アニメーションの前後で呼ぶ (VMDアニメーションの前後)
		virtual void BeginAnimation() = 0;
		virtual void EndAnimation() = 0;
		// Morph
		virtual void UpdateMorphAnimation() = 0;
		// ノードを更新する
		[[deprecated("Please use UpdateAllAnimation() function")]]
		void UpdateAnimation();
		virtual void UpdateNodeAnimation(bool afterPhysicsAnim) = 0;
		// Physicsを更新する
		virtual void ResetPhysics() = 0;
		[[deprecated("Please use UpdateAllAnimation() function")]]
		void UpdatePhysics(float elapsed);
		virtual void UpdatePhysicsAnimation(float elapsed) = 0;
		// 頂点を更新する
		virtual void Update() = 0;
		virtual void SetParallelUpdateHint(uint32_t parallelCount) = 0;

		void UpdateAllAnimation(VMDAnimation* vmdAnim, float vmdFrame, float physicsElapsed);
	};
}
