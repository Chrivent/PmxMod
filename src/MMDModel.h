#pragma once

#include "MMDMaterial.h"
#include "MMDMorph.h"
#include "MMDIkSolver.h"
#include "MMDPhysics.h"
#include "PMXFile.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <future>

namespace saba
{
	struct MMDMaterial;
	class MMDPhysics;
	class MMDRigidBody;
	class MMDJoint;
	struct VPDFile;

	class MMDNodeManager;
	class MMDIKManager;
	class MMDMorphManager;

	class MMDPhysicsManager;

	struct MMDSubMesh
	{
		int	m_beginIndex;
		int	m_vertexCount;
		int	m_materialID;
	};

	class VMDAnimation;

	struct VertexBoneInfo
	{
		PMXVertexWeight	m_skinningType;
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

	struct MaterialFactor
	{
		MaterialFactor() = default;
		explicit MaterialFactor(const MaterialMorph& pmxMat);

		void Mul(const MaterialFactor& val, float weight);
		void Add(const MaterialFactor& val, float weight);

		glm::vec3	m_diffuse{};
		float		m_alpha{};
		glm::vec3	m_specular{};
		float		m_specularPower{};
		glm::vec3	m_ambient{};
		glm::vec4	m_edgeColor{};
		float		m_edgeSize{};
		glm::vec4	m_textureFactor{};
		glm::vec4	m_spTextureFactor{};
		glm::vec4	m_toonTextureFactor{};
	};

	struct UpdateRange
	{
		size_t	m_vertexOffset;
		size_t	m_vertexCount;
	};

	class MMDModel
	{
	public:
		MMDModel();
		~MMDModel();

		// ノードを初期化する
		void InitializeAnimation();

		// ベースアニメーション(アニメーション読み込み時、Physics反映用)
		void SaveBaseAnimation() const;
		void LoadBaseAnimation() const;
		void ClearBaseAnimation() const;

		// アニメーションの前後で呼ぶ (VMDアニメーションの前後)
		void BeginAnimation();
		// Morph
		void UpdateMorphAnimation();
		// ノードを更新する
		void UpdateNodeAnimation(bool afterPhysicsAnim) const;
		// Physicsを更新する
		void ResetPhysics() const;
		void UpdatePhysicsAnimation(float elapsed) const;
		// 頂点を更新する
		void Update();

		void UpdateAllAnimation(const VMDAnimation* vmdAnim, float vmdFrame, float physicsElapsed);

		bool Load(const std::string& filepath, const std::string& mmdDataDir);
		void Destroy();

	private:
		void SetupParallelUpdate();
		void Update(const UpdateRange& range);

		void Morph(const MMDMorph* morph, float weight);

		void MorphPosition(const std::vector<PositionMorph>& morphData, float weight);

		void MorphUV(const std::vector<UVMorph>& morphData, float weight);

		void BeginMorphMaterial();
		void EndMorphMaterial();
		void MorphMaterial(const std::vector<MaterialMorph>& morphData, float weight);
		void MorphBone(const std::vector<BoneMorph>& morphData, float weight) const;

	public:
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

		std::vector<std::vector<PositionMorph>>	m_positionMorphDatas;
		std::vector<std::vector<UVMorph>>		m_uvMorphDatas;
		std::vector<std::vector<MaterialMorph>>	m_materialMorphDatas;
		std::vector<std::vector<BoneMorph>>		m_boneMorphDatas;
		std::vector<std::vector<GroupMorph>>		m_groupMorphDatas;

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
