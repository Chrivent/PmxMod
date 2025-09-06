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

		size_t FindNodeIndex(const std::string& name);
		MMDNode* GetNode(size_t idx);
		MMDNode* GetNode(const std::string& nodeName);
		MMDNode* AddNode();

		std::vector<std::unique_ptr<MMDNode>>	m_nodes;
	};

	class MMDIKManager
	{
	public:
		static const size_t NPos = -1;

		size_t FindIKSolverIndex(const std::string& name);
		MMDIkSolver* GetIKSolver(size_t idx);
		MMDIkSolver* GetIKSolver(const std::string& ikName);
		MMDIkSolver* AddIKSolver();

		std::vector<std::unique_ptr<MMDIkSolver>>	m_ikSolvers;
	};

	class MMDMorphManager
	{
	public:
		static const size_t NPos = -1;

		size_t FindMorphIndex(const std::string& name);
		MMDMorph* GetMorph(size_t idx);
		MMDMorph* GetMorph(const std::string& name);
		MMDMorph* AddMorph();

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
		MMDJoint* AddJoint();

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
