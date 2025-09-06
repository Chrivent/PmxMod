#pragma once

#include "MMDMaterial.h"
#include "MMDModel.h"
#include "MMDIkSolver.h"
#include "PMXFile.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <future>

namespace saba
{
	class PMXModel : public MMDModel
	{
	public:
		~PMXModel();

		MMDNodeManager* GetNodeManager() override { return &m_nodeMan; }
		MMDIKManager* GetIKManager() override { return &m_ikSolverMan; }
		MMDMorphManager* GetMorphManager() override { return &m_morphMan; };
		MMDPhysicsManager* GetPhysicsManager() override { return &m_physicsMan; }

		size_t GetVertexCount() const override { return m_positions.size(); }
		const glm::vec3* GetPositions() const override { return m_positions.data(); }
		const glm::vec3* GetNormals() const override { return m_normals.data(); }
		const glm::vec2* GetUVs() const override { return m_uvs.data(); }
		const glm::vec3* GetUpdatePositions() const override { return m_updatePositions.data(); }
		const glm::vec3* GetUpdateNormals() const override { return m_updateNormals.data(); }
		const glm::vec2* GetUpdateUVs() const override { return m_updateUVs.data(); }

		size_t GetIndexElementSize() const override { return m_indexElementSize; }
		size_t GetIndexCount() const override { return m_indexCount; }
		const void* GetIndices() const override { return &m_indices[0]; }

		size_t GetMaterialCount() const override { return m_materials.size(); }
		const MMDMaterial* GetMaterials() const override { return &m_materials[0]; }

		size_t GetSubMeshCount() const override { return m_subMeshes.size(); }
		const MMDSubMesh* GetSubMeshes() const override { return &m_subMeshes[0]; }

		MMDPhysics* GetMMDPhysics() override { return m_physicsMan.GetMMDPhysics(); }

		void InitializeAnimation() override;
		// アニメーションの前後で呼ぶ (VMDアニメーションの前後)
		void BeginAnimation() override;
		void EndAnimation() override;
		// Morph
		void UpdateMorphAnimation() override;
		// ノードを更新する
		void UpdateNodeAnimation(bool afterPhysicsAnim) override;
		// Physicsを更新する
		void ResetPhysics() override;
		void UpdatePhysicsAnimation(float elapsed) override;
		// 頂点データーを更新する
		void Update() override;
		void SetParallelUpdateHint(uint32_t parallelCount) override;

		bool Load(const std::string& filepath, const std::string& mmdDataDir);
		void Destroy();

		const glm::vec3& GetBBoxMin() const { return m_bboxMin; }
		const glm::vec3& GetBBoxMax() const { return m_bboxMax; }

	private:
		void SetupParallelUpdate();
		void Update(const UpdateRange& range);

		void Morph(MMDMorph* morph, float weight);

		void MorphPosition(const PositionMorphData& morphData, float weight);

		void MorphUV(const UVMorphData& morphData, float weight);

		void BeginMorphMaterial();
		void EndMorphMaterial();
		void MorphMaterial(const MaterialMorphData& morphData, float weight);

		void MorphBone(const BoneMorphData& morphData, float weight);
	};
}
