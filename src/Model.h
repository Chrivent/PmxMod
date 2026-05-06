#pragma once

#include <future>
#include <functional>

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
class Animation;

enum class SphereMode : uint8_t;
enum class MorphType : uint8_t;
enum class WeightType : uint8_t;

struct SubMesh {
	int	m_beginIndex;
	int	m_indexCount;
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

	const std::vector<glm::vec3>& GetPositions() const { return m_positions; }
	const std::vector<glm::vec3>& GetUpdatePositions() const { return m_updatePositions; }
	const std::vector<glm::vec3>& GetUpdateNormals() const { return m_updateNormals; }
	const std::vector<glm::vec2>& GetUpdateUVs() const { return m_updateUVs; }
	const std::vector<char>& GetIndices() const { return m_indices; }
	size_t GetIndexCount() const { return m_indexCount; }
	size_t GetIndexElementSize() const { return m_indexElementSize; }
	const std::vector<Material>& GetMaterials() const { return m_materials; }
	const std::vector<SubMesh>& GetSubMeshes() const { return m_subMeshes; }
	const std::vector<std::shared_ptr<Node>>& GetNodes() const { return m_nodes; }
	const std::vector<std::shared_ptr<IkSolver>>& GetIkSolvers() const { return m_ikSolvers; }
	const std::vector<std::unique_ptr<Morph>>& GetMorphs() const { return m_morphs; }

	/// 애니메이션 평가에 필요한 기본 상태를 초기화한다.
	void InitializeAnimation();
	/// 현재 애니메이션 상태를 기준 애니메이션으로 저장한다.
	void SaveBaseAnimation() const;
	/// 저장된 기준 애니메이션 상태를 지운다.
	void ClearBaseAnimation() const;
	/// 프레임 애니메이션 평가를 시작하기 전 상태를 준비한다.
	void BeginAnimation();
	/// 현재 모프 가중치를 반영해 모프 애니메이션을 갱신한다.
	void UpdateMorphAnimation();
	/// 노드 애니메이션과 IK를 갱신한다.
	void UpdateNodeAnimation(bool afterPhysicsAnim) const;
	/// 물리 월드의 강체와 제약 상태를 초기 위치로 되돌린다.
	void ResetPhysics() const;
	/// 경과 시간만큼 물리 시뮬레이션을 진행하고 본에 반영한다.
	void UpdatePhysicsAnimation(float elapsed) const;
	/// 모델의 전체 변형 결과를 현재 상태 기준으로 갱신한다.
	void Update();
	/// 지정한 애니메이션 프레임과 물리 시간으로 모든 애니메이션 단계를 갱신한다.
	void UpdateAllAnimation(const Animation* anim, float frame, float physicsElapsed);
	/// PMX 모델과 관련 리소스를 파일에서 로드한다.
	bool Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir);
	/// 모델이 소유한 리소스와 런타임 상태를 해제한다.
	void Destroy();

private:
	std::string									m_modelName;
	std::string									m_englishModelName;
	std::string									m_comment;
	std::string									m_englishComment;
	std::vector<glm::vec3>						m_positions;
	std::vector<glm::vec3>						m_normals;
	std::vector<glm::vec2>						m_uvs;
	std::vector<Vertex>							m_vertexBoneInfos;
	std::vector<glm::vec3>						m_updatePositions;
	std::vector<glm::vec3>						m_updateNormals;
	std::vector<glm::vec2>						m_updateUVs;
	std::vector<glm::mat4>						m_transforms;
	std::vector<char>							m_indices;
	size_t										m_indexCount = 0;
	size_t										m_indexElementSize = 0;
	std::vector<std::vector<PositionMorph>>		m_positionMorphDatas;
	std::vector<std::vector<UVMorph>>			m_uvMorphDatas;
	std::vector<std::vector<MaterialMorph>>		m_materialMorphDatas;
	std::vector<std::vector<BoneMorph>>			m_boneMorphDatas;
	std::vector<std::vector<GroupMorph>>		m_groupMorphDatas;
	std::vector<glm::vec3>						m_morphPositions;
	std::vector<glm::vec4>						m_morphUVs;
	std::vector<Material>						m_initMaterials;
	std::vector<MaterialMorph>					m_mulMaterialFactors;
	std::vector<MaterialMorph>					m_addMaterialFactors;
	glm::vec3									m_bboxMin;
	glm::vec3									m_bboxMax;
	std::vector<Material>						m_materials;
	std::vector<SubMesh>						m_subMeshes;
	std::vector<std::reference_wrapper<Node>>	m_sortedNodes;
	std::vector<std::shared_ptr<Node>>			m_nodes;
	std::vector<std::shared_ptr<IkSolver>>		m_ikSolvers;
	std::vector<std::unique_ptr<Morph>>			m_morphs;
	std::unique_ptr<Physics>					m_physics;
	std::vector<std::unique_ptr<RigidBody>>		m_rigidBodies;
	std::vector<std::unique_ptr<Joint>>			m_joints;
	uint32_t									m_parallelUpdateCount = 0;
	std::vector<UpdateRange>					m_updateRanges;
	std::vector<std::future<void>>				m_parallelUpdateFutures;

	/// 병렬 버텍스 갱신에 사용할 작업 범위를 구성한다.
	void SetupParallelUpdate();
	/// 지정된 버텍스 범위의 스키닝 결과를 갱신한다.
	void Update(const UpdateRange& range);
	/// 단일 모프를 지정한 가중치로 평가한다.
	void EvalMorph(const Morph* morph, float weight);
	/// 위치 모프 데이터를 버텍스 위치에 적용한다.
	void MorphPosition(const std::vector<PositionMorph>& morphData, float weight);
	/// UV 모프 데이터를 버텍스 UV에 적용한다.
	void MorphUV(const std::vector<UVMorph>& morphData, float weight);
	/// 재질 모프 누적을 시작하기 위해 재질 계수를 초기화한다.
	void BeginMorphMaterial();
	/// 누적된 재질 모프 결과를 최종 재질에 반영한다.
	void EndMorphMaterial();
	/// 재질 모프 데이터를 현재 재질 계수에 누적한다.
	void MorphMaterial(const std::vector<MaterialMorph>& morphData, float weight);
	/// 본 모프 데이터를 노드 애니메이션 변환에 적용한다.
	void MorphBone(const std::vector<BoneMorph>& morphData, float weight) const;
};
