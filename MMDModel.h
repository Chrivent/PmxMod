#pragma once

#include "MMDNode.h"
#include "MMDMaterial.h"
#include "MMDIkSolver.h"
#include "MMDMorph.h"
#include "PMXFile.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <future>

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
		MMDModel();

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

		enum class SkinningType
		{
			Weight1,
			Weight2,
			Weight4,
			SDEF,
			DualQuaternion,
		};
		struct VertexBoneInfo
		{
			SkinningType	m_skinningType;
			union
			{
				struct
				{
					int32_t	m_boneIndex[4];
					float	m_boneWeight[4];
				};
				struct
				{
					int32_t	m_boneIndex[2];
					float	m_boneWeight;

					glm::vec3	m_sdefC;
					glm::vec3	m_sdefR0;
					glm::vec3	m_sdefR1;
				} m_sdef;
			};
		};

	protected: //Todo private
		struct PositionMorph
		{
			uint32_t	m_index;
			glm::vec3	m_position;
		};

		struct PositionMorphData
		{
			std::vector<PositionMorph>	m_morphVertices;
		};

		struct UVMorph
		{
			uint32_t	m_index;
			glm::vec4	m_uv;
		};

		struct UVMorphData
		{
			std::vector<UVMorph>	m_morphUVs;
		};

		struct MaterialFactor
		{
			MaterialFactor() = default;
			MaterialFactor(const saba::PMXMorph::MaterialMorph& pmxMat);

			void Mul(const MaterialFactor& val, float weight);
			void Add(const MaterialFactor& val, float weight);

			glm::vec3	m_diffuse;
			float		m_alpha;
			glm::vec3	m_specular;
			float		m_specularPower;
			glm::vec3	m_ambient;
			glm::vec4	m_edgeColor;
			float		m_edgeSize;
			glm::vec4	m_textureFactor;
			glm::vec4	m_spTextureFactor;
			glm::vec4	m_toonTextureFactor;
		};

		struct MaterialMorphData
		{
			std::vector<saba::PMXMorph::MaterialMorph>	m_materialMorphs;
		};

		struct BoneMorphElement
		{
			MMDNode* m_node;
			glm::vec3	m_position;
			glm::quat	m_rotate;
		};

		struct BoneMorphData
		{
			std::vector<BoneMorphElement>	m_boneMorphs;
		};

		struct GroupMorphData
		{
			std::vector<saba::PMXMorph::GroupMorph>		m_groupMorphs;
		};

		struct UpdateRange
		{
			size_t	m_vertexOffset;
			size_t	m_vertexCount;
		};

	protected: //Todo public
		std::vector<glm::vec3>	m_positions;
		std::vector<glm::vec3>	m_normals;
		std::vector<glm::vec2>	m_uvs;
		std::vector<VertexBoneInfo>	m_vertexBoneInfos;
		std::vector<glm::vec3>	m_updatePositions;
		std::vector<glm::vec3>	m_updateNormals;
		std::vector<glm::vec2>	m_updateUVs;
		std::vector<glm::mat4>	m_transforms;

		std::vector<char>	m_indices;
		size_t				m_indexCount;
		size_t				m_indexElementSize;

		std::vector<PositionMorphData>	m_positionMorphDatas;
		std::vector<UVMorphData>		m_uvMorphDatas;
		std::vector<MaterialMorphData>	m_materialMorphDatas;
		std::vector<BoneMorphData>		m_boneMorphDatas;
		std::vector<GroupMorphData>		m_groupMorphDatas;

		// PositionMorph用
		std::vector<glm::vec3>	m_morphPositions;
		std::vector<glm::vec4>	m_morphUVs;

		// マテリアルMorph用
		std::vector<MMDMaterial>	m_initMaterials;
		std::vector<MaterialFactor>	m_mulMaterialFactors;
		std::vector<MaterialFactor>	m_addMaterialFactors;

		glm::vec3		m_bboxMin;
		glm::vec3		m_bboxMax;

		std::vector<MMDMaterial>	m_materials;
		std::vector<MMDSubMesh>		m_subMeshes;
		std::vector<MMDNode*>		m_sortedNodes;

		MMDNodeManager				m_nodeMan;
		MMDIKManager				m_ikSolverMan;
		MMDMorphManager				m_morphMan;
		MMDPhysicsManager			m_physicsMan;

		uint32_t							m_parallelUpdateCount;
		std::vector<UpdateRange>			m_updateRanges;
		std::vector<std::future<void>>		m_parallelUpdateFutures;
	};
}
