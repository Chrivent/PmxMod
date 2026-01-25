#pragma once

#include <future>

#include "Node.h"
#include "IkSolver.h"
#include "Physics.h"

struct GroupMorph;
struct BoneMorph;
struct UVMorph;
struct PositionMorph;
struct MaterialMorph;
struct RigidBody;
struct Joint;
struct Animation;

enum class SphereMode : uint8_t;
enum class MorphType : uint8_t;
enum class WeightType : uint8_t;

struct SubMesh {
	int	m_beginIndex;
	int	m_vertexCount;
	int	m_materialID;
};

struct Vertex {
	WeightType	m_weightType;
	int32_t		m_boneIndices[4];
	float		m_boneWeights[4];
	glm::vec3	m_sdefC;
	glm::vec3	m_sdefR0;
	glm::vec3	m_sdefR1;
};

struct Morph {
	std::string	m_name;
	float		m_weight = 0;
	float		m_saveAnimWeight = 0;
	MorphType	m_morphType;
	size_t		m_dataIndex = 0;
};

struct Material {
	glm::vec4				m_diffuse = glm::vec4(1);
	glm::vec3				m_specular = glm::vec3(0);
	float					m_specularPower = 1;
	glm::vec3				m_ambient = glm::vec3(0.2f);
	uint8_t					m_edgeFlag = 0;
	float					m_edgeSize = 0;
	glm::vec4				m_edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	std::filesystem::path	m_texture;
	std::filesystem::path	m_spTexture;
	SphereMode				m_spTextureMode = SphereMode::None;
	std::filesystem::path	m_toonTexture;
	glm::vec4				m_textureMulFactor = glm::vec4(1);
	glm::vec4				m_spTextureMulFactor = glm::vec4(1);
	glm::vec4				m_toonTextureMulFactor = glm::vec4(1);
	glm::vec4				m_textureAddFactor = glm::vec4(0);
	glm::vec4				m_spTextureAddFactor = glm::vec4(0);
	glm::vec4				m_toonTextureAddFactor = glm::vec4(0);
	bool					m_bothFace = false;
	bool					m_groundShadow = true;
	bool					m_shadowCaster = true;
	bool					m_shadowReceiver = true;
};

struct UpdateRange {
	size_t m_vertexOffset;
	size_t m_vertexCount;
};

class Model {
public:
	~Model();

	std::string								m_modelName;
	std::string								m_englishModelName;
	std::string								m_comment;
	std::string								m_englishComment;
	std::vector<glm::vec3>					m_positions;
	std::vector<glm::vec3>					m_normals;
	std::vector<glm::vec2>					m_uvs;
	std::vector<Vertex>						m_vertexBoneInfos;
	std::vector<glm::vec3>					m_updatePositions;
	std::vector<glm::vec3>					m_updateNormals;
	std::vector<glm::vec2>					m_updateUVs;
	std::vector<glm::mat4>					m_transforms;
	std::vector<char>						m_indices;
	size_t									m_indexCount = 0;
	size_t									m_indexElementSize = 0;
	std::vector<std::vector<PositionMorph>>	m_positionMorphDatas;
	std::vector<std::vector<UVMorph>>		m_uvMorphDatas;
	std::vector<std::vector<MaterialMorph>>	m_materialMorphDatas;
	std::vector<std::vector<BoneMorph>>		m_boneMorphDatas;
	std::vector<std::vector<GroupMorph>>	m_groupMorphDatas;
	std::vector<glm::vec3>					m_morphPositions;
	std::vector<glm::vec4>					m_morphUVs;
	std::vector<Material>					m_initMaterials;
	std::vector<MaterialMorph>				m_mulMaterialFactors;
	std::vector<MaterialMorph>				m_addMaterialFactors;
	glm::vec3								m_bboxMin;
	glm::vec3								m_bboxMax;
	std::vector<Material>					m_materials;
	std::vector<SubMesh>					m_subMeshes;
	std::vector<Node*>						m_sortedNodes;
	std::vector<std::unique_ptr<Node>>		m_nodes;
	std::vector<std::unique_ptr<IkSolver>>	m_ikSolvers;
	std::vector<std::unique_ptr<Morph>>		m_morphs;
	std::unique_ptr<Physics>				m_physics;
	std::vector<std::unique_ptr<RigidBody>>	m_rigidBodies;
	std::vector<std::unique_ptr<Joint>>		m_joints;
	uint32_t								m_parallelUpdateCount = 0;
	std::vector<UpdateRange>				m_updateRanges;
	std::vector<std::future<void>>			m_parallelUpdateFutures;

	void InitializeAnimation();
	void SaveBaseAnimation() const;
	void ClearBaseAnimation() const;
	void BeginAnimation();
	void UpdateMorphAnimation();
	void UpdateNodeAnimation(bool afterPhysicsAnim) const;
	void ResetPhysics() const;
	void UpdatePhysicsAnimation(float elapsed) const;
	void Update();
	void UpdateAllAnimation(const Animation* anim, float frame, float physicsElapsed);
	bool Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir);
	void Destroy();

private:
	void SetupParallelUpdate();
	void Update(const UpdateRange& range);
	void EvalMorph(const Morph* morph, float weight);
	void MorphPosition(const std::vector<PositionMorph>& morphData, float weight);
	void MorphUV(const std::vector<UVMorph>& morphData, float weight);
	void BeginMorphMaterial();
	void EndMorphMaterial();
	void MorphMaterial(const std::vector<MaterialMorph>& morphData, float weight);
	void MorphBone(const std::vector<BoneMorph>& morphData, float weight) const;
};
